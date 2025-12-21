#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFiManager.h>


class ConfigManager {
public:
  static void begin();
  static void reset();

  // Getters for stored values
  static String getCity();
  static String getBusStop();
  static String getBusLine();

  // Callbacks
  static void saveConfigCallback();
  static bool shouldSaveConfig;

private:
  static Preferences prefs;
  static String city;
  static String busStop;
  static String busLine;
};
