#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// All members have defaults: a fetch that only partially succeeds must never
// leave garbage behind (WeatherData is created fresh for every fetch).
struct DailyForecast {
  String date; // "YYYY-MM-DD" (local to the city); empty = no data
  float maxTemp = 0;
  float minTemp = 0;
  int weatherCode = -1; // WMO code, -1 = unknown
  float pop = 0;        // Probability of Precipitation (0..1)
};

struct HourlyForecast {
  String time; // "YYYY-MM-DD HH:MM" (local to the city); empty = no data
  float temp = 0;
  int weatherCode = -1;
  float pop = 0;        // Probability of Precipitation (0..1)
  bool isNight = false; // For moon / night-cloud icons
};

struct WeatherData {
  String cityName;
  float currentTemp = 0;
  int currentWeatherCode = -1;
  int currentHumidity = 0;
  float currentPressure = 0;
  float currentFeelsLike = 0;
  int currentAQI = 0; // OWM scale 1..5, 0 = unknown
  float windSpeed = 0; // km/h
  int windDirection = 0;
  float currentRainProb = 0;
  bool isNight = false;
  String sunrise; // "HH:MM" city-local, empty = unknown
  String sunset;
  float uvIndex = -1; // Today's max UV index, -1 = unknown
  int hourlyStepHours = 1; // 1 = Open-Meteo hourly, 3 = OWM 3-hour slots
  uint32_t lastUpdate = 0; // millis() of last successful update, 0 = no data
  DailyForecast daily[7];
  HourlyForecast hourly[24];
};

class WeatherService {
public:
  static bool updateWeather(WeatherData &data, float lat, float lon,
                            const String &owmApiKey = "");
  // Uses OWM geocoding when a key is given, falling back to the keyless
  // Open-Meteo geocoder. cityName may carry a country code: "Paris,FR".
  static bool lookupCoordinates(const String &cityName, float &lat,
                                float &lon, String &resolvedName,
                                const String &apiKey);
  static const char *getAQIDesc(int aqi);

private:
  static bool lookupCoordinatesOWM(const String &cityName, float &lat,
                                   float &lon, String &resolvedName,
                                   const String &apiKey);
  static bool lookupCoordinatesOpenMeteo(const String &cityName, float &lat,
                                         float &lon, String &resolvedName);
  static bool updateForecastOpenMeteo(WeatherData &data, float lat, float lon);
  static bool updateForecastOWM_5Day(WeatherData &data, float lat, float lon,
                                     const String &apiKey);
  static bool updateCurrentWeatherOWM(WeatherData &data, float lat, float lon,
                                      const String &apiKey);
  // OWM only: UV, full-day "today" high/low and the days OWM lacks
  static void supplementOpenMeteoDaily(WeatherData &data, float lat,
                                       float lon);
  static void updateAirQualityOWM(WeatherData &data, float lat, float lon,
                                  const String &apiKey);
};
