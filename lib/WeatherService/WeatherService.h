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
  float pop; // Probability of Precipitation (0..1)
};

struct HourlyForecast {
  String time;
  float temp;
  int weatherCode;
  float pop; // Probability of Precipitation (0..1)
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
  float currentRainProb; // New
  bool isNight;          // New: For icon selection
  DailyForecast daily[7];
  HourlyForecast hourly[24];
};

class WeatherService {
public:
  static bool updateWeather(WeatherData &data, float lat, float lon,
                            String owmApiKey = "");
  static bool lookupCoordinates(String cityName, float &lat, float &lon,
                                String &resolvedName, String apiKey);
  static const char *getAQIDesc(int aqi);

private:
  static bool updateCurrentWeatherOWM(WeatherData &data, float lat, float lon,
                                      String apiKey);
  static bool updateForecastOWM_5Day(WeatherData &data, float lat, float lon,
                                     String apiKey);
};
