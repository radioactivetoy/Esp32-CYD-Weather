#include "ConfigManager.h"

Preferences ConfigManager::prefs;
bool ConfigManager::shouldSaveConfig = false;

// Default Defaults (first run only)
String ConfigManager::city = "Barcelona";
String ConfigManager::busStop = "2156";
String ConfigManager::busLine = "V15";

void ConfigManager::saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void ConfigManager::begin() {
  // 1. Load from NVS
  prefs.begin("weather_cfg", false);
  city = prefs.getString("city", "Barcelona");
  busStop = prefs.getString("busStop", "2156");
  busLine = prefs.getString("busLine", "V15");

  Serial.println("Loaded Config:");
  Serial.println("City: " + city);
  Serial.println("Bus Stop: " + busStop);
  Serial.println("Bus Line: " + busLine);

  // 2. Setup WiFiManager
  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);

  // Custom Params
  WiFiManagerParameter custom_city("city", "City Name", city.c_str(), 32);
  WiFiManagerParameter custom_busStop("busStop", "Bus Stop Info",
                                      busStop.c_str(), 10);
  WiFiManagerParameter custom_busLine("busLine", "Bus Line ID", busLine.c_str(),
                                      6);

  wm.addParameter(&custom_city);
  wm.addParameter(&custom_busStop);
  wm.addParameter(&custom_busLine);

  // 3. Connect or AP
  // Use a unique AP name
  if (!wm.autoConnect("WeatherClockAP")) {
    Serial.println("Failed to connect and hit timeout");
    // Only happens if timeout is set, usually it blocks or resets
    ESP.restart();
    delay(1000);
  }

  Serial.println("WiFi Connected!");

  // 4. Save if Updated
  if (shouldSaveConfig) {
    Serial.println("Saving config...");
    city = custom_city.getValue();
    busStop = custom_busStop.getValue();
    busLine = custom_busLine.getValue();

    prefs.putString("city", city);
    prefs.putString("busStop", busStop);
    prefs.putString("busLine", busLine);
    Serial.println("Config Saved.");
  }

  prefs.end();
}

void ConfigManager::reset() {
  WiFiManager wm;
  wm.resetSettings();
  prefs.begin("weather_cfg", false);
  prefs.clear();
  prefs.end();
  Serial.println("Settings Reset.");
}

String ConfigManager::getCity() { return city; }
String ConfigManager::getBusStop() { return busStop; }
String ConfigManager::getBusLine() { return busLine; }
