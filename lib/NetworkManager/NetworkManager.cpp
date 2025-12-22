#include "NetworkManager.h"
#include <WiFiManager.h>

Preferences NetworkManager::prefs;
bool NetworkManager::shouldSaveConfig = false;
String NetworkManager::city = "Barcelona";
String NetworkManager::busStop = "2156";
String NetworkManager::timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
bool NetworkManager::nightMode = false;
int NetworkManager::nightStart = 22;
int NetworkManager::nightEnd = 7;
String NetworkManager::stockSymbols = "AAPL,BTC-USD,GRF.MC";
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

  // Improvements
  html += "<h3>Improvements</h3>";
  html += "Timezone:<br><select name='timezone'>";

  struct TZ {
    const char *name;
    const char *val;
  };
  TZ timezones[] = {
      // Africa
      {"Africa/Cairo (EET)", "EET-2EEST,M4.5.3/0,M10.5.4/24"},
      {"Africa/Johannesburg (SAST)", "SAST-2"},
      {"Africa/Lagos (WAT)", "WAT-1"},

      // Americas
      {"America/Anchorage (AKST)", "AKST9AKDT,M3.2.0,M11.1.0"},
      {"America/Argentina/Buenos_Aires (ART)", "ART3"},
      {"America/Bogota (COT)", "COT5"},
      {"America/Chicago (CST)", "CST6CDT,M3.2.0,M11.1.0"},
      {"America/Denver (MST)", "MST7MDT,M3.2.0,M11.1.0"},
      {"America/Los_Angeles (PST)", "PST8PDT,M3.2.0,M11.1.0"},
      {"America/Mexico_City (CST)", "CST6"},
      {"America/New_York (EST)", "EST5EDT,M3.2.0,M11.1.0"},
      {"America/Phoenix (MST)", "MST7"},
      {"America/Sao_Paulo (BRT)", "BRT3"},
      {"America/Toronto (EST)", "EST5EDT,M3.2.0,M11.1.0"},
      {"America/Vancouver (PST)", "PST8PDT,M3.2.0,M11.1.0"},

      // Asia
      {"Asia/Bangkok (ICT)", "ICT-7"},
      {"Asia/Dubai (GST)", "GST-4"},
      {"Asia/Hong_Kong (HKT)", "HKT-8"},
      {"Asia/Jakarta (WIB)", "WIB-7"},
      {"Asia/Jerusalem (IST)", "IST-2IDT,M3.4.4/26,M10.5.0"},
      {"Asia/Kolkata (IST)", "IST-5:30"},
      {"Asia/Seoul (KST)", "KST-9"},
      {"Asia/Shanghai (CST)", "CST-8"},
      {"Asia/Singapore (SGT)", "SGT-8"},
      {"Asia/Tokyo (JST)", "JST-9"},

      // Europe
      {"Europe/Amsterdam (CET)", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Athens (EET)", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
      {"Europe/Berlin (CET)", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Brussels (CET)", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Helsinki (EET)", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
      {"Europe/Istanbul (TRT)", "TRT-3"},
      {"Europe/Kyiv (EET)", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
      {"Europe/Lisbon (WET)", "WET0WEST,M3.5.0/1,M10.5.0"},
      {"Europe/London (GMT)", "GMT0BST,M3.5.0/1,M10.5.0"},
      {"Europe/Madrid (CET)", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Moscow (MSK)", "MSK-3"},
      {"Europe/Paris (CET)", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Rome (CET)", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Stockholm (CET)", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Zurich (CET)", "CET-1CEST,M3.5.0,M10.5.0/3"},

      // Pacific
      {"Pacific/Auckland (NZST)", "NZST-12NZDT,M9.5.0/2,M4.1.0/3"},
      {"Pacific/Fiji (FJT)", "FJT-12"},
      {"Pacific/Honolulu (HST)", "HST10"},
      {"Australia/Sydney (AEST)", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
      {"Australia/Perth (AWST)", "AWST-8"},

      // UTC
      {"UTC", "GMT0"}};

  for (const auto &tz : timezones) {
    String selected = (String(tz.val) == timezone) ? " selected" : "";
    html += "<option value='" + String(tz.val) + "'" + selected + ">" +
            String(tz.name) + "</option>";
  }
  html += "</select><br>";

  String checked = nightMode ? "checked" : "";
  html += "Night Mode (Auto-Dim): <input type='checkbox' name='nightMode' " +
          checked + "><br>";

  html += "Night Start (Hour 0-23):<br><input type='number' name='nightStart' "
          "value='" +
          String(nightStart) + "'><br>";
  "Night End (Hour 0-23):<br><input type='number' name='nightEnd' value='" +
      String(nightEnd) + "'><br><br>";

  html += "<h3>Stock Ticker</h3>";
  html += "Symbols (comma split):<br><input type='text' name='stockSymbols' "
          "value='" +
          stockSymbols + "'><br><br>";

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

    // New Params
    timezone = server.arg("timezone");
    nightMode = server.hasArg("nightMode"); // Checkbox present = true
    nightStart = server.arg("nightStart").toInt();
    nightEnd = server.arg("nightEnd").toInt();

    prefs.begin("weather_cfg", false);
    prefs.putString("city", city);
    prefs.putString("busStop", busStop);
    prefs.putString("app_id", appId);
    prefs.putString("app_key", appKey);

    prefs.putString("timezone", timezone);
    prefs.putBool("nightMode", nightMode);
    prefs.putInt("nightStart", nightStart);
    prefs.putInt("nightEnd", nightEnd);

    // Stocks
    stockSymbols = server.arg("stockSymbols");
    prefs.putString("stockSymbols", stockSymbols);

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
  sprintf(buf, "Connect to AP:\n%s\nIP: 192.168.4.1",
          myWiFiManager->getConfigPortalSSID().c_str());
  // GuiController::showLoadingScreen(buf); // Decoupled to avoid circular dep
  Serial.println(buf);
}

void NetworkManager::begin() {
  Serial.println("NETWORK: Begin...");
  prefs.begin("weather_cfg", false);
  city = prefs.getString("city", "Barcelona");
  busStop = prefs.getString("busStop", "2156");
  String savedAppId = prefs.getString("app_id", "");
  String savedAppKey = prefs.getString("app_key", "");

  // Load Improvements
  timezone = prefs.getString("timezone", "CET-1CEST,M3.5.0,M10.5.0/3");
  nightMode = prefs.getBool("nightMode", false);
  nightStart = prefs.getInt("nightStart", 22);
  nightEnd = prefs.getInt("nightEnd", 7);

  // Load Stocks
  stockSymbols = prefs.getString("stockSymbols", "AAPL,BTC-USD,GRF.MC");

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

  // Init NTP
  configTime(3600, 3600, "pool.ntp.org"); // GMT+1 + Daylight Saving
  // More specific for Barcelona:
  // More specific for Barcelona (OR Configured):
  setenv("TZ", timezone.c_str(), 1);
  tzset();

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
    // Note: Improvements not in WiFiManager yet for simplicity, default to NVS
    // load If you want them in CP, add WiFiManagerParameters. But WebUI is
    // better for advanced stuff.
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

String NetworkManager::getTimezone() { return timezone; }
bool NetworkManager::getNightModeEnabled() { return nightMode; }
int NetworkManager::getNightStart() { return nightStart; }
int NetworkManager::getNightEnd() { return nightEnd; }
String NetworkManager::getStockSymbols() { return stockSymbols; }
