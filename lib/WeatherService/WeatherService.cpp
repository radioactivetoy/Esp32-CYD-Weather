#include "WeatherService.h"

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

bool WeatherService::updateWeather(WeatherData &data, float lat, float lon) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  bool weatherSuccess = false;
  // 1. Weather Forecast
  {
    HTTPClient http;
    String url = "http://api.open-meteo.com/v1/forecast?latitude=" +
                 String(lat) + "&longitude=" + String(lon) +
                 "&current=temperature_2m,relative_humidity_2m,apparent_"
                 "temperature,"
                 "pressure_msl,weather_code,wind_speed_10m,wind_direction_10m" +
                 "&daily=weather_code,temperature_2m_max,temperature_2m_min" +
                 "&hourly=temperature_2m,weather_code&timezone=auto&past_days="
                 "1"; // Added past_days=1

    Serial.println("Fetching weather: " + url);
    http.begin(url);
    http.setConnectTimeout(5000);
    http.setTimeout(5000);

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        weatherSuccess = true;
        JsonObject current = doc["current"];
        data.currentTemp = current["temperature_2m"];
        data.currentHumidity = current["relative_humidity_2m"];
        data.currentPressure = current["pressure_msl"];
        data.currentFeelsLike = current["apparent_temperature"];
        data.currentWeatherCode = current["weather_code"];
        data.windSpeed = current["wind_speed_10m"];
        data.windDirection = current["wind_direction_10m"];

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
    HTTPClient http;
    String aqiUrl =
        "http://air-quality-api.open-meteo.com/v1/air-quality?latitude=" +
        String(lat) + "&longitude=" + String(lon) + "&current=european_aqi";

    Serial.println("Fetching AQI: " + aqiUrl);
    http.begin(aqiUrl);
    int aqiRes = http.GET();
    if (aqiRes > 0) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        data.currentAQI = doc["current"]["european_aqi"];
      }
    }
    http.end();
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

  HTTPClient http;
  // URL Encode city name
  String encodedCity = cityName;
  encodedCity.replace(" ", "%20");

  String url =
      "https://geocoding-api.open-meteo.com/v1/search?name=" + encodedCity +
      "&count=1&language=en&format=json";

  Serial.println("Geocoding city: " + url);
  http.begin(url);
  http.setConnectTimeout(5000);
  http.setTimeout(5000);

  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

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
