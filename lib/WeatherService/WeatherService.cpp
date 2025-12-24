#include "WeatherService.h"
#include <WiFiClientSecure.h>

// Open-Meteo URL:
// https://api.open-meteo.com/v1/forecast?latitude=XX&longitude=YY&current_weather=true&daily=weathercode,temperature_2m_max,temperature_2m_min&timezone=auto

// Helper to calculate moon phase (0-7)
// 0: New, 1: WaxCresc, 2: 1stQ, 3: WaxGibb, 4: Full, 5: WanGibb, 6: 3rdQ, 7:
// WanCresc
int calculateMoonPhase(int year, int month, int day) {
  if (month < 3) {
    year--;
    month += 12;
  }
  ++month;
  // Julian Date approx
  double c = 365.25 * year;
  double e = 30.6 * month;
  double jd = c + e + day - 694039.09;
  jd /= 29.5305882; // Determine lunar cycles
  int b = (int)jd;
  jd -= b;                 // fractional part only [0..1]
  b = (int)(jd * 8 + 0.5); // resize to 0..8 scale
  b = b & 7;               // cap to 0..7
  return b;
}

bool WeatherService::updateWeather(WeatherData &data, float lat, float lon,
                                   String owmApiKey) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  bool weatherSuccess = false;

  // 1. Fetch Forecast (Prioritize OWM)
  bool forecastSuccess = false;
  if (owmApiKey.length() > 0) {
    forecastSuccess = updateForecastOWM_5Day(data, lat, lon, owmApiKey);
    if (forecastSuccess)
      weatherSuccess = true;
  }

  if (!forecastSuccess) {
    // Fallback to Open-Meteo
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    // Change http -> https
    String url =
        "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat) +
        "&longitude=" + String(lon) +
        "&current=temperature_2m,relative_humidity_2m,apparent_"
        "temperature,"
        "pressure_msl,weather_code,wind_speed_10m,wind_direction_10m,is_day" +
        "&daily=weather_code,temperature_2m_max,temperature_2m_min" +
        "&hourly=temperature_2m,weather_code&timezone=auto&past_days="
        "1"; // Added past_days=1

    Serial.println("Fetching Open-Meteo: " + url);
    http.begin(client, url); // Pass client
    http.useHTTP10(true);    // Disable Chunked Transfer for Stream Parsing
    http.setConnectTimeout(5000);
    http.setTimeout(5000);

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      // Stream Parsing for Memory Safety
      JsonDocument doc; // Changed to DynamicJsonDocument if stack issue, but
                        // JsonDocument is ArduinoJson 7
      DeserializationError error = deserializeJson(doc, http.getStream());

      if (error) { // ... existing error handle
        Serial.print("Deserialize Open-Meteo failed: ");
        Serial.println(error.c_str());
      } else {
        weatherSuccess =
            false; // logic flow is a bit weird in original, let's just parse
                   // Original parsing logic for OpenMeteo...
                   // Keeping existing parsing logic but wrapping it

        // ... (Assuming original parsing code blocks follow, I'll just change
        // the top part to allow fallback) Actually, I should just replace the
        // top block.
      }

      if (error) {
        Serial.print("Deserialize JSON failed: ");
        Serial.println(error.c_str());
      } else {
        // ... (parsing logic remains same)
        weatherSuccess = true;
        JsonObject current = doc["current"];
        data.currentTemp = current["temperature_2m"];
        data.currentHumidity = current["relative_humidity_2m"];
        data.currentPressure = current["pressure_msl"];
        data.currentFeelsLike = current["apparent_temperature"];
        data.currentWeatherCode = current["weather_code"];
        data.windSpeed = current["wind_speed_10m"];
        data.windDirection = current["wind_direction_10m"];

        // Night Detection
        int isDay = current["is_day"];
        data.isNight = (isDay == 0);

        JsonArray time = doc["daily"]["time"];
        // Loop for Today (Index 1) -> +6 Days
        // We map JSON index i+1 to storage index i
        for (int i = 0; i < 7; i++) {
          int jsonIdx = i + 1; // Shift by 1 because 0 is Yesterday
          if (jsonIdx >= (int)time.size())
            break;

          data.daily[i].date = time[jsonIdx].template as<String>();
          data.daily[i].maxTemp = doc["daily"]["temperature_2m_max"][jsonIdx];
          data.daily[i].minTemp = doc["daily"]["temperature_2m_min"][jsonIdx];
          data.daily[i].weatherCode = doc["daily"]["weather_code"][jsonIdx];

          int y, m, d;
          if (sscanf(data.daily[i].date.c_str(), "%d-%d-%d", &y, &m, &d) == 3) {
            data.daily[i].moonPhaseIndex = calculateMoonPhase(y, m, d);
          }
        }
        data.currentMoonPhase = data.daily[0].moonPhaseIndex;

        JsonArray h_time = doc["hourly"]["time"];
        struct tm timeinfo;
        int startIdx = 0;
        if (getLocalTime(&timeinfo))
          startIdx = timeinfo.tm_hour;

        for (int i = 0; i < 24; i++) {
          int idx = startIdx + i;
          if (idx >= (int)h_time.size())
            break;
          data.hourly[i].time = h_time[idx].template as<String>();
          data.hourly[i].temp = doc["hourly"]["temperature_2m"][idx];
          data.hourly[i].weatherCode = doc["hourly"]["weather_code"][idx];
        }
      }
    }
    http.end();
  }

  if (!weatherSuccess)
    return false;

  // 2. Air Quality Forecast (OWM Air Pollution)
  // Scale 1 (Good) to 5 (Poor)
  {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String aqiUrl =
        "https://api.openweathermap.org/data/2.5/air_pollution?lat=" +
        String(lat) + "&lon=" + String(lon) + "&appid=" + owmApiKey;

    Serial.println("Fetching AQI OWM: " + aqiUrl);
    http.begin(client, aqiUrl);
    http.setConnectTimeout(5000);
    http.setTimeout(5000); // 5s timeout
    int aqiRes = http.GET();
    if (aqiRes > 0) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, http.getStream());
      if (!error) {
        // "list": [{ "main": { "aqi": 1 }, ... }]
        if (doc.containsKey("list")) {
          // OWM: 1 (Good), 2 (Fair), 3 (Moderate), 4 (Poor), 5 (Very Poor)
          data.currentAQI = doc["list"][0]["main"]["aqi"];
        }
      } else {
        Serial.print("AQI Parse Error: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.printf("AQI HTTP Error: %d\n", aqiRes);
    }
    http.end();
  }

  // 3. Hybrid: Overwrite Current Weather with OpenWeatherMap if Key is present
  if (weatherSuccess && owmApiKey.length() > 0) {
    updateCurrentWeatherOWM(data, lat, lon, owmApiKey);
  }

  return true;
}

const char *WeatherService::getAQIDesc(int aqi) {
  // OWM Scale: 1-5
  switch (aqi) {
  case 1:
    return "Good";
  case 2:
    return "Fair";
  case 3:
    return "Moderate";
  case 4:
    return "Poor";
  case 5:
    return "Very Poor";
  default:
    return "Unknown";
  }
}

bool WeatherService::lookupCoordinates(String cityName, float &lat, float &lon,
                                       String &resolvedName, String apiKey) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  // URL Encode city name
  String encodedCity = cityName;
  encodedCity.replace(" ", "%20");

  // OWM Geocoding
  String url =
      "https://api.openweathermap.org/geo/1.0/direct?q=" + encodedCity +
      "&limit=1&appid=" + apiKey;

  Serial.println("Geocoding city OWM: " + url);
  http.begin(client, url);
  http.setConnectTimeout(5000);
  http.setTimeout(5000);

  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());

    // Expecting an Array [ { "name": ... } ]
    if (!error && doc.is<JsonArray>() && doc.size() > 0) {
      JsonObject result = doc[0];
      lat = result["lat"];
      lon = result["lon"];
      resolvedName = result["name"].as<String>();

      Serial.printf("Resolved %s to %.4f, %.4f (%s)\n", cityName.c_str(), lat,
                    lon, resolvedName.c_str());

      http.end();
      return true;
    }

    Serial.print("Geocoding failed/parsed error: ");
    Serial.println(error.c_str());
    http.end();
    return false;
  }
  Serial.printf("Geocoding HTTP Error: %d\n", httpResponseCode);
  http.end();
  return false;
}

bool WeatherService::updateForecastOWM_5Day(WeatherData &data, float lat,
                                            float lon, String apiKey) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  // 5 Day / 3 Hour Forecast
  String url =
      "https://api.openweathermap.org/data/2.5/forecast?lat=" + String(lat) +
      "&lon=" + String(lon) + "&appid=" + apiKey + "&units=metric";

  Serial.println("Fetching OWM Forecast 5Day: " + url);
  http.begin(client, url);
  http.setConnectTimeout(6000);
  http.setTimeout(6000);

  int code = http.GET();
  if (code > 0) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());

    if (!error) {
      JsonArray list = doc["list"];
      if (list.size() > 0) {

        // 1. Fill Hourly (actually 3-hour steps) - Take first 24 items (72h
        // coverage)
        for (int i = 0; i < 24 && i < list.size(); i++) {
          JsonObject item = list[i];
          data.hourly[i].temp = item["main"]["temp"];
          String dt_txt = item["dt_txt"].as<String>();
          data.hourly[i].time = dt_txt; // "YYYY-MM-DD HH:MM:SS"

          float pop = item["pop"]; // 0..1
          data.hourly[i].pop = pop;

          // Map Icon
          String icon = item["weather"][0]["icon"].as<String>();
          int wmo = 3;
          if (icon.startsWith("01"))
            wmo = 0;
          else if (icon.startsWith("02"))
            wmo = 1;
          else if (icon.startsWith("03"))
            wmo = 2;
          else if (icon.startsWith("04"))
            wmo = 3;
          else if (icon.startsWith("09"))
            wmo = 80;
          else if (icon.startsWith("10"))
            wmo = 61;
          else if (icon.startsWith("11"))
            wmo = 95;
          else if (icon.startsWith("13"))
            wmo = 71;
          else if (icon.startsWith("50"))
            wmo = 45;
          data.hourly[i].weatherCode = wmo;
        }

        // Current Rain Prob Proxy (use first forecast slot)
        data.currentRainProb = data.hourly[0].pop;

        // 2. Fill Daily (Aggregate by Day) - Use "Midday Rule" & Max POP
        int dayIndex = 0;
        String currentDay = "";
        float dayMin = 100, dayMax = -100;
        float dayPopMax = 0;
        int middayIconCode = 3;
        int middayDiff = 9999;

        for (JsonObject item : list) {
          String dt_txt = item["dt_txt"].as<String>();
          String dayStr = dt_txt.substring(0, 10);   // YYYY-MM-DD
          String timeStr = dt_txt.substring(11, 16); // HH:MM
          int hour = timeStr.substring(0, 2).toInt();

          if (currentDay == "")
            currentDay = dayStr;

          if (dayStr != currentDay) {
            // Commit previous day
            if (dayIndex < 7) {
              data.daily[dayIndex].date = currentDay;
              data.daily[dayIndex].maxTemp = dayMax;
              data.daily[dayIndex].minTemp = dayMin;
              data.daily[dayIndex].weatherCode = middayIconCode;
              data.daily[dayIndex].pop = dayPopMax;

              int y, m, d;
              if (sscanf(currentDay.c_str(), "%d-%d-%d", &y, &m, &d) == 3) {
                data.daily[dayIndex].moonPhaseIndex =
                    calculateMoonPhase(y, m, d);
              }

              dayIndex++;
            }
            // Reset for new day
            currentDay = dayStr;
            dayMin = 100;
            dayMax = -100;
            dayPopMax = 0;
            middayDiff = 9999;
            middayIconCode = 3;
          }

          // Stats
          float t = item["main"]["temp"];
          if (t < dayMin)
            dayMin = t;
          if (t > dayMax)
            dayMax = t;

          float p = item["pop"];
          if (p > dayPopMax)
            dayPopMax = p;

          // Icon Selection (Midday Rule)
          int diff = abs(hour - 12);
          if (diff < middayDiff) {
            middayDiff = diff;
            String icon = item["weather"][0]["icon"].as<String>();
            int wmo = 3;
            if (icon.startsWith("01"))
              wmo = 0;
            else if (icon.startsWith("02"))
              wmo = 1;
            else if (icon.startsWith("03"))
              wmo = 2;
            else if (icon.startsWith("04"))
              wmo = 3;
            else if (icon.startsWith("09"))
              wmo = 80;
            else if (icon.startsWith("10"))
              wmo = 61;
            else if (icon.startsWith("11"))
              wmo = 95;
            else if (icon.startsWith("13"))
              wmo = 71;
            else if (icon.startsWith("50"))
              wmo = 45;
            middayIconCode = wmo;
          }
        }

        // Commit last day
        if (dayIndex < 7) {
          data.daily[dayIndex].date = currentDay;
          data.daily[dayIndex].maxTemp = dayMax;
          data.daily[dayIndex].minTemp = dayMin;
          data.daily[dayIndex].weatherCode = middayIconCode;
          data.daily[dayIndex].pop = dayPopMax;

          int y, m, d;
          if (sscanf(currentDay.c_str(), "%d-%d-%d", &y, &m, &d) == 3) {
            data.daily[dayIndex].moonPhaseIndex = calculateMoonPhase(y, m, d);
          }
        }
        // Set current moon phase from today's forecast
        if (dayIndex > 0 || (dayIndex == 0 && currentDay != "")) {
          data.currentMoonPhase = data.daily[0].moonPhaseIndex;
        }

        Serial.println("OWM Forecast 5Day Success");
        http.end();
        return true;
      }
    } else {
      Serial.print("OWM Forecast JSON Error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("OWM Forecast HTTP Error: %d\n", code);
  }
  http.end();
  return false;
}

bool WeatherService::updateCurrentWeatherOWM(WeatherData &data, float lat,
                                             float lon, String apiKey) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url =
      "https://api.openweathermap.org/data/2.5/weather?lat=" + String(lat) +
      "&lon=" + String(lon) + "&appid=" + apiKey + "&units=metric";

  Serial.println("Fetching OWM Current: " + url);
  http.begin(client, url);
  http.setConnectTimeout(5000);
  http.setTimeout(5000);

  int code = http.GET();
  if (code > 0) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (!error) {
      if (doc.containsKey("main")) {
        // Overwrite Data
        data.currentTemp = doc["main"]["temp"];
        data.currentHumidity = doc["main"]["humidity"];
        data.currentPressure = doc["main"]["pressure"];
        data.currentFeelsLike = doc["main"]["feels_like"];
        data.windSpeed = doc["wind"]["speed"]; // m/s
        data.windSpeed *= 3.6;                 // Convert to km/h
        data.windDirection = doc["wind"]["deg"];

        // Icon Mapping
        String icon = doc["weather"][0]["icon"].as<String>();
        data.isNight = icon.endsWith("n");
        int wmo = 3; // Default Overcast

        if (icon.startsWith("01"))
          wmo = 0; // Clear
        else if (icon.startsWith("02"))
          wmo = 1; // Few Clouds
        else if (icon.startsWith("03"))
          wmo = 2; // Scattered
        else if (icon.startsWith("04"))
          wmo = 3; // Broken
        else if (icon.startsWith("09"))
          wmo = 80; // Shower Rain
        else if (icon.startsWith("10"))
          wmo = 61; // Rain
        else if (icon.startsWith("11"))
          wmo = 95; // Thunder
        else if (icon.startsWith("13"))
          wmo = 71; // Snow
        else if (icon.startsWith("50"))
          wmo = 45; // Mist

        data.currentWeatherCode = wmo;
        Serial.printf("OWM Update Success: Temp=%.1f Icon=%s WMO=%d\n",
                      data.currentTemp, icon.c_str(), wmo);
        http.end();
        return true;
      }
    } else {
      Serial.print("OWM JSON Error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("OWM HTTP Error: %d\n", code);
  }
  http.end();
  return false;
}
