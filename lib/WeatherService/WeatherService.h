#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

struct DailyForecast {
  String date;
  float maxTemp;
  float minTemp;
  int weatherCode;
  int moonPhaseIndex;
};

struct HourlyForecast {
  String time;
  float temp;
  int weatherCode;
};

struct WeatherData {
  String cityName;
  float currentTemp;
  int currentWeatherCode;
  int currentHumidity;
  float currentPressure;
  float currentFeelsLike;
  int currentMoonPhase;
  int currentAQI;
  float windSpeed;
  int windDirection;
  bool isNight;           // New: For icon selection
  float yesterdayMaxTemp; // Added for Main Screen Trend
  DailyForecast daily[7];
  HourlyForecast hourly[24];
};

class WeatherService {
public:
  static bool updateWeather(WeatherData &data, float lat, float lon,
                            String owmApiKey = "");
  static bool lookupCoordinates(String cityName, float &lat, float &lon,
                                String &resolvedName);
  static const char *getAQIDesc(int aqi);

private:
  static bool updateCurrentWeatherOWM(WeatherData &data, float lat, float lon,
                                      String apiKey);
};
