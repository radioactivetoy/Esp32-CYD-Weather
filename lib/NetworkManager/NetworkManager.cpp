#include "NetworkManager.h"
#include "GuiController.h"
#include <WiFiManager.h>

Preferences NetworkManager::prefs;
bool NetworkManager::shouldSaveConfig = false;
String NetworkManager::city = "Barcelona";
String NetworkManager::busStop = "2156";
WebServer NetworkManager::server(80);

void NetworkManager::saveConfigCallback() { shouldSaveConfig = true; }

void NetworkManager::handleClient() { server.handleClient(); }

void NetworkManager::handleRoot() {
  String html = "<html><head><title>Weather Clock Settings</title>";
  html +=
      "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:sans-serif;max-width:500px;margin:20px "
          "auto;padding:20px;background:#1a1a1a;color:white;}";
  html += "input{width:100%;padding:10px;margin:5px 0;box-sizing:border-box;}";
  html += "input[type=submit]{background:#007bff;color:white;border:none;"
          "cursor:pointer;}";
  html += "h2{border-bottom:1px solid #444;padding-bottom:10px;}";
  html += "</style></head><body>";
  html += "<h2>Device Config</h2>";
  html += "<form action='/save' method='POST'>";

  // Current values
  String appId = getAppId();
  String appKey = getAppKey();

  html +=
      "City Name:<br><input type='text' name='city' value='" + city + "'><br>";
  html += "Bus Stop ID:<br><input type='text' name='busStop' value='" +
          busStop + "'><br>";
  html += "TMB App ID:<br><input type='text' name='appId' value='" + appId +
          "'><br>";
  html += "TMB App Key:<br><input type='text' name='appKey' value='" + appKey +
          "'><br><br>";

  html += "<input type='submit' value='Save & Reboot'></form>";
  html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void NetworkManager::handleSave() {
  if (server.hasArg("city") && server.hasArg("busStop")) {
    city = server.arg("city");
    busStop = server.arg("busStop");
    String appId = server.arg("appId");
    String appKey = server.arg("appKey");

    prefs.begin("weather_cfg", false);
    prefs.putString("city", city);
    prefs.putString("busStop", busStop);
    prefs.putString("app_id", appId);
    prefs.putString("app_key", appKey);
    prefs.end();

    String html = "<html><head><meta http-equiv='refresh' "
                  "content='3;url=/'></head><body>";
    html += "<h2>Saved!</h2><p>Restarting...</p></body></html>";
    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void NetworkManager::configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("NETWORK: Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());

  char buf[64];
  snprintf(buf, sizeof(buf), "AP Active: %s",
           myWiFiManager->getConfigPortalSSID().c_str());

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    GuiController::showLoadingScreen(buf);
    xSemaphoreGive(dataMutex);
  }
}

void NetworkManager::begin() {
  Serial.println("NETWORK: Begin...");
  prefs.begin("weather_cfg", false);
  city = prefs.getString("city", "Barcelona");
  busStop = prefs.getString("busStop", "2156");
  String savedAppId = prefs.getString("app_id", "");
  String savedAppKey = prefs.getString("app_key", "");

  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setAPCallback(configModeCallback); // Show when in AP mode

  // Custom Parameters
  // id/name, placeholder/prompt, default, length
  WiFiManagerParameter custom_city("city", "City Name", city.c_str(), 32);
  WiFiManagerParameter custom_busStop("busStop", "Bus Stop ID", busStop.c_str(),
                                      10);
  WiFiManagerParameter custom_appId("appId", "TMB App ID", savedAppId.c_str(),
                                    32);
  WiFiManagerParameter custom_appKey("appKey", "TMB App Key",
                                     savedAppKey.c_str(), 64);

  wm.addParameter(&custom_city);
  wm.addParameter(&custom_busStop);
  wm.addParameter(&custom_appId);
  wm.addParameter(&custom_appKey);

  // Set timeout
  wm.setConfigPortalTimeout(180);

  Serial.println("NETWORK: Attempting AutoConnect...");
  if (!wm.autoConnect("WeatherClockAP")) {
    Serial.println("NETWORK: Failed to connect and hit timeout");
    ESP.restart();
  }
  Serial.println("NETWORK: WiFi Connected!");

  if (shouldSaveConfig) {
    Serial.println("NETWORK: Saving New Config...");
    city = custom_city.getValue();
    busStop = custom_busStop.getValue();
    savedAppId = custom_appId.getValue();
    savedAppKey = custom_appKey.getValue();

    Serial.printf("NETWORK: New City: %s, BusStop: %s\n", city.c_str(),
                  busStop.c_str());

    prefs.putString("city", city);
    prefs.putString("busStop", busStop);
    prefs.putString("app_id", savedAppId);
    prefs.putString("app_key", savedAppKey);
  }
  prefs.end();

  // Start Web Server
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(
      []() { server.send(404, "text/plain", "Not Found"); }); // Catch-all
  server.begin();
  Serial.println("NETWORK: Web Server Started.");

  Serial.println("NETWORK: Setup Complete.");
}

void NetworkManager::reset() {
  WiFiManager wm;
  wm.resetSettings();
  prefs.begin("weather_cfg", false);
  prefs.clear();
  prefs.end();
}

String NetworkManager::getCity() { return city; }
String NetworkManager::getBusStop() { return busStop; }

String NetworkManager::getAppId() {
  prefs.begin("weather_cfg", true);
  String val = prefs.getString("app_id", "");
  prefs.end();
  return val;
}

String NetworkManager::getAppKey() {
  prefs.begin("weather_cfg", true);
  String val = prefs.getString("app_key", "");
  prefs.end();
  return val;
}
