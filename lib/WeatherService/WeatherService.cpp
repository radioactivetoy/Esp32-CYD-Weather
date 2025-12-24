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

  // 1. Weather Forecast
  {
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

    Serial.println("Fetching weather: " + url);
    http.begin(client, url); // Pass client
    http.useHTTP10(true);    // Disable Chunked Transfer for Stream Parsing
    http.setConnectTimeout(5000);
    http.setTimeout(5000);

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      // Stream Parsing for Memory Safety
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, http.getStream());

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

        // Capture Yesterday's Max (Index 0)
        data.yesterdayMaxTemp = doc["daily"]["temperature_2m_max"][0];

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

  // 2. Air Quality Forecast
  {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String aqiUrl =
        "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" +
        String(lat) + "&longitude=" + String(lon) + "&current=european_aqi";

    Serial.println("Fetching AQI: " + aqiUrl);
    http.begin(client, aqiUrl);
    http.useHTTP10(true); // Disable Chunked Transfer for Stream Parsing
    int aqiRes = http.GET();
    if (aqiRes > 0) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, http.getStream());
      if (!error) {
        data.currentAQI = doc["current"]["european_aqi"];
      }
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
  if (aqi <= 20)
    return "Excellent";
  if (aqi <= 40)
    return "Good";
  if (aqi <= 60)
    return "Moderate";
  if (aqi <= 80)
    return "Poor";
  if (aqi <= 100)
    return "Unhealthy";
  return "Hazardous";
}

bool WeatherService::lookupCoordinates(String cityName, float &lat, float &lon,
                                       String &resolvedName) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  // URL Encode city name
  String encodedCity = cityName;
  encodedCity.replace(" ", "%20");

  String url =
      "https://geocoding-api.open-meteo.com/v1/search?name=" + encodedCity +
      "&count=1&language=en&format=json";

  Serial.println("Geocoding city: " + url);
  http.begin(client, url);
  http.useHTTP10(true); // Disable Chunked Transfer for Stream Parsing
  http.setConnectTimeout(5000);
  http.setTimeout(5000);

  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    // Use Stream to save RAM
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());

    if (error || !doc.containsKey("results")) {
      Serial.println("Geocoding failed or no results.");
      http.end();
      return false;
    }

    JsonObject result = doc["results"][0];
    lat = result["latitude"];
    lon = result["longitude"];
    resolvedName = result["name"].as<String>();

    Serial.printf("Resolved %s to %.4f, %.4f (%s)\n", cityName.c_str(), lat,
                  lon, resolvedName.c_str());

    http.end();
    return true;
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
