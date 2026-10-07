#pragma once

#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <vector>

class NetworkManager {
public:
  static void begin();
  static void reset();

  static void handleClient(); // New: Handle web requests

  static String getCity();
  static String getBusStop();
  static String getAppId();
  static String getAppKey();

  // New: Improvements
  static String getTimezone();
  static bool getNightModeEnabled();
  static int getNightStart();
  static int getNightEnd();
  static String getOwmApiKey();

  static int getDayBrightness();
  static int getNightBrightness();

  static String getStockSymbols();
  static String getLedBrightness();
  static std::vector<String> getBusStops(); // Split "2156,1234"
  // Split "Barcelona,Madrid", or "Paris,FR;London,GB" when ';' is used
  static std::vector<String> getCities();

  // Legacy method if used
  static bool isConnected() { return WiFi.status() == WL_CONNECTED; }

private:
  static Preferences prefs;
  static WebServer server; // New: Web Server instance
  static String city;
  static String busStop;
  static String appId;
  static String appKey;
  static String timezone;
  static bool nightMode;
  static int nightStart;
  static int nightEnd;
  static String owmApiKey;
  static int dayBrightness;
  static int nightBrightness;
  static String stockSymbols;
  static String ledBrightness;
  static String webPassword; // Optional HTTP basic auth for the settings page
  static bool shouldSaveConfig;
  static bool checkAuth();
  static void saveConfigCallback();
  static void configModeCallback(WiFiManager *myWiFiManager);

  // New: Web Handlers
  static void handleRoot();
  static void handleSave();
};
