#include "WeatherService.h"
#include "NetUtils.h"
#include <WiFiClientSecure.h>
#include <time.h>

// Map an OWM icon id ("01d", "10n", ...) to the closest WMO weather code.
static int owmIconToWmo(const char *icon) {
  if (!icon || strlen(icon) < 2)
    return 3;
  if (strncmp(icon, "01", 2) == 0)
    return 0; // Clear
  if (strncmp(icon, "02", 2) == 0)
    return 1; // Few clouds
  if (strncmp(icon, "03", 2) == 0)
    return 2; // Scattered
  if (strncmp(icon, "04", 2) == 0)
    return 3; // Broken / overcast
  if (strncmp(icon, "09", 2) == 0)
    return 80; // Shower rain
  if (strncmp(icon, "10", 2) == 0)
    return 61; // Rain
  if (strncmp(icon, "11", 2) == 0)
    return 95; // Thunder
  if (strncmp(icon, "13", 2) == 0)
    return 71; // Snow
  if (strncmp(icon, "50", 2) == 0)
    return 45; // Mist
  return 3;
}

static bool isNightIcon(const char *icon) {
  size_t len = icon ? strlen(icon) : 0;
  return len > 0 && icon[len - 1] == 'n';
}

// True once NTP has set the clock (anything after 2023).
static bool clockIsSet() { return time(nullptr) > 1700000000; }

// Breaks a UTC epoch shifted by the city's UTC offset into local fields.
static void toCityLocal(time_t utc, long offsetSec, struct tm &out) {
  time_t t = utc + offsetSec;
  gmtime_r(&t, &out);
}

// Opens an HTTPS GET; returns true only on HTTP 200.
static bool httpsGet(HTTPClient &http, WiFiClientSecure &client,
                     const String &url, uint16_t timeoutMs) {
  client.setInsecure();
  if (!http.begin(client, url))
    return false;
  http.useHTTP10(true); // No chunked encoding: we parse straight from the stream
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("WEATHER: HTTP error %d\n", code);
    return false;
  }
  return true;
}

bool WeatherService::updateWeather(WeatherData &data, float lat, float lon,
                                   const String &owmApiKey) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  bool hasKey = owmApiKey.length() > 0;

  // 1. Forecast: prefer OWM, fall back to keyless Open-Meteo
  bool forecastOk = false;
  if (hasKey)
    forecastOk = updateForecastOWM_5Day(data, lat, lon, owmApiKey);
  if (!forecastOk)
    forecastOk = updateForecastOpenMeteo(data, lat, lon);
  if (!forecastOk)
    return false;

  if (hasKey) {
    // 2. Air quality (OWM only)
    updateAirQualityOWM(data, lat, lon, owmApiKey);
    // 3. More accurate current conditions. If this fails, the forecast
    //    already filled in current values from the nearest slot.
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

// ---------------------------------------------------------------------------
// Geocoding
// ---------------------------------------------------------------------------

bool WeatherService::lookupCoordinates(const String &cityName, float &lat,
                                       float &lon, String &resolvedName,
                                       const String &apiKey) {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  if (apiKey.length() > 0 &&
      lookupCoordinatesOWM(cityName, lat, lon, resolvedName, apiKey))
    return true;
  return lookupCoordinatesOpenMeteo(cityName, lat, lon, resolvedName);
}

bool WeatherService::lookupCoordinatesOWM(const String &cityName, float &lat,
                                          float &lon, String &resolvedName,
                                          const String &apiKey) {
  WiFiClientSecure client;
  HTTPClient http;
  String url = "https://api.openweathermap.org/geo/1.0/direct?q=" +
               NetUtils::urlEncode(cityName) + "&limit=1&appid=" + apiKey;

  Serial.printf("Geocoding (OWM): %s\n", cityName.c_str());
  bool ok = false;
  if (httpsGet(http, client, url, 5000)) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    // Expecting an array: [ { "name": ..., "lat": ..., "lon": ... } ]
    if (!error && doc.is<JsonArray>() && doc.size() > 0) {
      JsonObject result = doc[0];
      lat = result["lat"];
      lon = result["lon"];
      resolvedName = result["name"] | "";
      ok = true;
    } else {
      Serial.printf("Geocoding (OWM) failed: %s\n",
                    error ? error.c_str() : "no results");
    }
  }
  http.end();
  client.stop();
  if (ok)
    Serial.printf("Resolved %s to %.4f, %.4f (%s)\n", cityName.c_str(), lat,
                  lon, resolvedName.c_str());
  return ok;
}

bool WeatherService::lookupCoordinatesOpenMeteo(const String &cityName,
                                                float &lat, float &lon,
                                                String &resolvedName) {
  // Open-Meteo has no "City,CC" syntax; split off a country code if present.
  String name = cityName;
  String country;
  int comma = cityName.indexOf(',');
  if (comma >= 0) {
    name = cityName.substring(0, comma);
    country = cityName.substring(comma + 1);
    name.trim();
    country.trim();
  }

  WiFiClientSecure client;
  HTTPClient http;
  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" +
               NetUtils::urlEncode(name) + "&count=1&language=en&format=json";
  if (country.length() == 2)
    url += "&countryCode=" + country;

  Serial.printf("Geocoding (Open-Meteo): %s\n", cityName.c_str());
  bool ok = false;
  if (httpsGet(http, client, url, 5000)) {
    JsonDocument filter;
    filter["results"][0]["name"] = true;
    filter["results"][0]["latitude"] = true;
    filter["results"][0]["longitude"] = true;
    JsonDocument doc;
    DeserializationError error = deserializeJson(
        doc, http.getStream(), DeserializationOption::Filter(filter));
    JsonArray results = doc["results"];
    if (!error && results.size() > 0) {
      lat = results[0]["latitude"];
      lon = results[0]["longitude"];
      resolvedName = results[0]["name"] | "";
      ok = true;
    } else {
      Serial.printf("Geocoding (Open-Meteo) failed: %s\n",
                    error ? error.c_str() : "no results");
    }
  }
  http.end();
  client.stop();
  if (ok)
    Serial.printf("Resolved %s to %.4f, %.4f (%s)\n", cityName.c_str(), lat,
                  lon, resolvedName.c_str());
  return ok;
}

// ---------------------------------------------------------------------------
// Open-Meteo (keyless fallback)
// ---------------------------------------------------------------------------

bool WeatherService::updateForecastOpenMeteo(WeatherData &data, float lat,
                                             float lon) {
  WiFiClientSecure client;
  HTTPClient http;
  String url =
      "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
      "&longitude=" + String(lon, 4) +
      "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
      "pressure_msl,weather_code,wind_speed_10m,wind_direction_10m,is_day"
      "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
      "precipitation_probability_max,sunrise,sunset"
      "&hourly=temperature_2m,weather_code,precipitation_probability,is_day"
      "&timezone=auto&past_days=1";

  Serial.println("Fetching Open-Meteo: " + url);
  bool ok = false;
  if (httpsGet(http, client, url, 5000)) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) {
      Serial.print("Deserialize Open-Meteo failed: ");
      Serial.println(error.c_str());
    } else if (doc["current"].is<JsonObject>()) {
      ok = true;
      data.hourlyStepHours = 1;

      JsonObject current = doc["current"];
      data.currentTemp = current["temperature_2m"];
      data.currentHumidity = current["relative_humidity_2m"];
      data.currentPressure = current["pressure_msl"];
      data.currentFeelsLike = current["apparent_temperature"];
      data.currentWeatherCode = current["weather_code"] | -1;
      data.windSpeed = current["wind_speed_10m"];
      data.windDirection = current["wind_direction_10m"];
      data.isNight = (current["is_day"] | 1) == 0;

      // Daily: index 0 is yesterday (past_days=1), so shift by one.
      JsonArray dayTimes = doc["daily"]["time"];
      for (int i = 0; i < 7; i++) {
        int jsonIdx = i + 1;
        if (jsonIdx >= (int)dayTimes.size())
          break;
        data.daily[i].date = dayTimes[jsonIdx].as<String>();
        data.daily[i].maxTemp = doc["daily"]["temperature_2m_max"][jsonIdx];
        data.daily[i].minTemp = doc["daily"]["temperature_2m_min"][jsonIdx];
        data.daily[i].weatherCode =
            doc["daily"]["weather_code"][jsonIdx] | -1;
        data.daily[i].pop =
            (doc["daily"]["precipitation_probability_max"][jsonIdx] | 0) /
            100.0f;
      }

      // Today's sun times ("YYYY-MM-DDTHH:MM", already city-local); index 1
      // is today because of past_days=1
      String rise = doc["daily"]["sunrise"][1] | "";
      String set = doc["daily"]["sunset"][1] | "";
      if (rise.length() >= 16)
        data.sunrise = rise.substring(11, 16);
      if (set.length() >= 16)
        data.sunset = set.substring(11, 16);

      // Hourly: yesterday occupies 0..23, today starts at 24. Skip the hours
      // already past *in the city's own time zone*, not the device's.
      JsonArray h_time = doc["hourly"]["time"];
      int startIdx = 24;
      long offset = doc["utc_offset_seconds"] | 0L;
      struct tm local;
      if (clockIsSet()) {
        toCityLocal(time(nullptr), offset, local);
        startIdx = 24 + local.tm_hour;
      } else if (getLocalTime(&local, 10)) {
        startIdx = 24 + local.tm_hour;
      }

      for (int i = 0; i < 24; i++) {
        int idx = startIdx + i;
        if (idx >= (int)h_time.size())
          break;
        // "YYYY-MM-DDTHH:MM" -> "YYYY-MM-DD HH:MM"
        String t = h_time[idx].as<String>();
        t.replace("T", " ");
        data.hourly[i].time = t;
        data.hourly[i].temp = doc["hourly"]["temperature_2m"][idx];
        data.hourly[i].weatherCode = doc["hourly"]["weather_code"][idx] | -1;
        data.hourly[i].pop =
            (doc["hourly"]["precipitation_probability"][idx] | 0) / 100.0f;
        data.hourly[i].isNight = (doc["hourly"]["is_day"][idx] | 1) == 0;
      }
      data.currentRainProb = data.hourly[0].pop;
    } else {
      Serial.println("Open-Meteo: unexpected response");
    }
  }
  http.end();
  client.stop();
  return ok;
}

// ---------------------------------------------------------------------------
// OpenWeatherMap
// ---------------------------------------------------------------------------

bool WeatherService::updateForecastOWM_5Day(WeatherData &data, float lat,
                                            float lon, const String &apiKey) {
  WiFiClientSecure client;
  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/forecast?lat=" +
               String(lat, 4) + "&lon=" + String(lon, 4) + "&appid=" + apiKey +
               "&units=metric";

  Serial.printf("Fetching OWM Forecast 5Day: %.4f, %.4f\n", lat, lon);
  bool ok = false;
  if (httpsGet(http, client, url, 6000)) {
    // The full response is large; keep only the fields we use.
    JsonDocument filter;
    JsonObject f = filter["list"].add<JsonObject>();
    f["dt"] = true;
    f["main"]["temp"] = true;
    f["main"]["feels_like"] = true;
    f["main"]["humidity"] = true;
    f["main"]["pressure"] = true;
    f["pop"] = true;
    f["weather"][0]["icon"] = true;
    f["wind"]["speed"] = true;
    f["wind"]["deg"] = true;
    filter["city"]["timezone"] = true;
    filter["city"]["sunrise"] = true;
    filter["city"]["sunset"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(
        doc, http.getStream(), DeserializationOption::Filter(filter));
    JsonArray list = doc["list"];

    if (error) {
      Serial.print("OWM Forecast JSON Error: ");
      Serial.println(error.c_str());
    } else if (list.size() > 0) {
      ok = true;
      data.hourlyStepHours = 3;
      long tzOffset = doc["city"]["timezone"] | 0L;

      // Sun times (UTC epochs) -> city-local "HH:MM"
      long riseUtc = doc["city"]["sunrise"] | 0L;
      long setUtc = doc["city"]["sunset"] | 0L;
      if (riseUtc > 0 && setUtc > 0) {
        struct tm t;
        char hm[8];
        toCityLocal(riseUtc, tzOffset, t);
        strftime(hm, sizeof(hm), "%H:%M", &t);
        data.sunrise = hm;
        toCityLocal(setUtc, tzOffset, t);
        strftime(hm, sizeof(hm), "%H:%M", &t);
        data.sunset = hm;
      }

      // 1. "Hourly" list (3-hour steps). Times are converted to city-local.
      for (int i = 0; i < 24 && i < (int)list.size(); i++) {
        JsonObject item = list[i];
        struct tm local;
        toCityLocal(item["dt"].as<long>(), tzOffset, local);
        char buf[20];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &local);
        data.hourly[i].time = buf;
        data.hourly[i].temp = item["main"]["temp"];
        data.hourly[i].pop = item["pop"];
        const char *icon = item["weather"][0]["icon"] | "";
        data.hourly[i].weatherCode = owmIconToWmo(icon);
        data.hourly[i].isNight = isNightIcon(icon);
      }
      data.currentRainProb = data.hourly[0].pop;

      // Provisional current conditions from the nearest slot, so the screen
      // is sensible even if the "current weather" call fails afterwards.
      JsonObject first = list[0];
      const char *firstIcon = first["weather"][0]["icon"] | "";
      data.currentTemp = first["main"]["temp"];
      data.currentFeelsLike = first["main"]["feels_like"];
      data.currentHumidity = first["main"]["humidity"];
      data.currentPressure = first["main"]["pressure"];
      data.windSpeed = first["wind"]["speed"].as<float>() * 3.6f; // m/s->km/h
      data.windDirection = first["wind"]["deg"];
      data.currentWeatherCode = owmIconToWmo(firstIcon);
      data.isNight = isNightIcon(firstIcon);

      // 2. Daily aggregation by city-local date: min/max temp, max POP and
      //    the icon closest to midday.
      int dayIndex = -1;
      char currentDay[11] = "";
      int middayDiff = 9999;

      for (JsonObject item : list) {
        struct tm local;
        toCityLocal(item["dt"].as<long>(), tzOffset, local);
        char dayStr[11];
        strftime(dayStr, sizeof(dayStr), "%Y-%m-%d", &local);

        if (strcmp(dayStr, currentDay) != 0) {
          if (dayIndex + 1 >= 7)
            break;
          dayIndex++;
          strcpy(currentDay, dayStr);
          DailyForecast &d = data.daily[dayIndex];
          d.date = dayStr;
          d.minTemp = 100;
          d.maxTemp = -100;
          d.pop = 0;
          middayDiff = 9999;
        }

        DailyForecast &d = data.daily[dayIndex];
        float t = item["main"]["temp"];
        if (t < d.minTemp)
          d.minTemp = t;
        if (t > d.maxTemp)
          d.maxTemp = t;
        float p = item["pop"];
        if (p > d.pop)
          d.pop = p;

        int diff = abs(local.tm_hour - 12);
        if (diff < middayDiff) {
          middayDiff = diff;
          d.weatherCode = owmIconToWmo(item["weather"][0]["icon"] | "");
        }
      }
      Serial.println("OWM Forecast 5Day Success");
    }
  }
  http.end();
  client.stop();
  return ok;
}

bool WeatherService::updateCurrentWeatherOWM(WeatherData &data, float lat,
                                             float lon, const String &apiKey) {
  WiFiClientSecure client;
  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/weather?lat=" +
               String(lat, 4) + "&lon=" + String(lon, 4) + "&appid=" + apiKey +
               "&units=metric";

  Serial.printf("Fetching OWM Current: %.4f, %.4f\n", lat, lon);
  bool ok = false;
  if (httpsGet(http, client, url, 5000)) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) {
      Serial.print("OWM JSON Error: ");
      Serial.println(error.c_str());
    } else if (doc["main"].is<JsonObject>()) {
      data.currentTemp = doc["main"]["temp"];
      data.currentHumidity = doc["main"]["humidity"];
      data.currentPressure = doc["main"]["pressure"];
      data.currentFeelsLike = doc["main"]["feels_like"];
      data.windSpeed = doc["wind"]["speed"].as<float>() * 3.6f; // m/s->km/h
      data.windDirection = doc["wind"]["deg"];

      const char *icon = doc["weather"][0]["icon"] | "";
      data.isNight = isNightIcon(icon);
      data.currentWeatherCode = owmIconToWmo(icon);
      Serial.printf("OWM Update Success: Temp=%.1f Icon=%s WMO=%d\n",
                    data.currentTemp, icon, data.currentWeatherCode);
      ok = true;
    }
  }
  http.end();
  client.stop();
  return ok;
}

void WeatherService::updateAirQualityOWM(WeatherData &data, float lat,
                                         float lon, const String &apiKey) {
  WiFiClientSecure client;
  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/air_pollution?lat=" +
               String(lat, 4) + "&lon=" + String(lon, 4) + "&appid=" + apiKey;

  Serial.printf("Fetching AQI OWM: %.4f, %.4f\n", lat, lon);
  if (httpsGet(http, client, url, 5000)) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (!error) {
      // "list": [{ "main": { "aqi": 1 }, ... }] — 1 (Good) .. 5 (Very Poor)
      data.currentAQI = doc["list"][0]["main"]["aqi"] | 0;
    } else {
      Serial.print("AQI Parse Error: ");
      Serial.println(error.c_str());
    }
  }
  http.end();
  client.stop();
}
