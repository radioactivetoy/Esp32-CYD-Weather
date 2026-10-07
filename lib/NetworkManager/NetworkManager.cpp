#include "NetworkManager.h"
#include "NetUtils.h"
#include "SystemMonitor.h"
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>

// mDNS name for OTA uploads and the settings page: http://weatherclock.local
static const char *OTA_HOSTNAME = "weatherclock";

Preferences NetworkManager::prefs;
bool NetworkManager::shouldSaveConfig = false;
String NetworkManager::city = "Barcelona";
String NetworkManager::busStop = "2156";
String NetworkManager::appId = "";
String NetworkManager::appKey = "";
String NetworkManager::timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
bool NetworkManager::nightMode = false;
int NetworkManager::nightStart = 22;
int NetworkManager::nightEnd = 7;
int NetworkManager::dayBrightness = 100;
int NetworkManager::nightBrightness = 10;
String NetworkManager::stockSymbols = "AAPL,BTC-USD,GRF.MC";
String NetworkManager::ledBrightness = "medium";
String NetworkManager::owmApiKey = "";
String NetworkManager::webPassword = "";
void (*NetworkManager::statusCallback)(const char *msg) = nullptr;
WebServer NetworkManager::server(80);

String NetworkManager::getLedBrightness() { return ledBrightness; }
String NetworkManager::getOwmApiKey() { return owmApiKey; }
int NetworkManager::getDayBrightness() { return dayBrightness; }
int NetworkManager::getNightBrightness() { return nightBrightness; }

std::vector<String> NetworkManager::getBusStops() {
  std::vector<String> stops = NetUtils::splitList(busStop, ',', 5);
  if (stops.empty())
    stops.push_back("2156"); // Default
  return stops;
}

std::vector<String> NetworkManager::getCities() {
  // ';' lets city names carry a country code: "Paris,FR;Paris,US"
  char sep = city.indexOf(';') >= 0 ? ';' : ',';
  std::vector<String> cities = NetUtils::splitList(city, sep, 5);
  if (cities.empty())
    cities.push_back("Barcelona");
  return cities;
}

void NetworkManager::saveConfigCallback() { shouldSaveConfig = true; }

void NetworkManager::handleClient() {
  server.handleClient();
  ArduinoOTA.handle(); // Blocks for the whole transfer while an upload runs
}

// Over-the-air firmware updates (PlatformIO / Arduino IDE "espota").
// Protected by the settings password when one is set.
void NetworkManager::setupOTA() {
  static bool otaStarted = false;
  static int lastShownPct = -1;

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (webPassword.length() > 0)
    ArduinoOTA.setPassword(webPassword.c_str());

  ArduinoOTA.onStart([]() {
    otaStarted = true;
    lastShownPct = -1;
    Serial.println("OTA: Update started");
    if (statusCallback)
      statusCallback("Updating firmware...\n\nDo not unplug");
  });

  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    // The transfer blocks the network task: keep its watchdog fed
    esp_task_wdt_reset();
    int pct = total ? (int)((uint64_t)done * 100 / total) : 0;
    if (pct / 5 != lastShownPct / 5) { // Redraw every 5%
      lastShownPct = pct;
      char buf[64];
      snprintf(buf, sizeof(buf), "Updating firmware...\n\n%d%%\n\nDo not unplug",
               pct);
      if (statusCallback)
        statusCallback(buf);
    }
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("OTA: Update complete, rebooting");
    if (statusCallback)
      statusCallback("Update complete\n\nRestarting...");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA: Error %u\n", error);
    // Errors before onStart (e.g. a wrong password) leave the app untouched.
    // Once a transfer started, the loading screen replaced the app screen,
    // so restart cleanly (the running firmware is still intact).
    if (otaStarted) {
      if (statusCallback)
        statusCallback("Update failed\n\nRestarting...");
      delay(3000);
      ESP.restart();
    }
  });

  ArduinoOTA.begin(); // Also starts mDNS as weatherclock.local
  Serial.printf("NETWORK: OTA ready at %s.local (%s)%s\n", OTA_HOSTNAME,
                WiFi.localIP().toString().c_str(),
                webPassword.length() > 0 ? ", password protected" : "");
}

// Returns true when the request may proceed. With no password set the page
// stays open (as before); otherwise HTTP basic auth with user "admin".
bool NetworkManager::checkAuth() {
  if (webPassword.length() == 0)
    return true;
  if (server.authenticate("admin", webPassword.c_str()))
    return true;
  server.requestAuthentication();
  return false;
}

void NetworkManager::handleRoot() {
  if (!checkAuth())
    return;
  using NetUtils::htmlEscape;

  String html = "<html><head><title>Weather Clock Settings</title>";
  html +=
      "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:sans-serif;max-width:500px;margin:20 "
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

  html += "City Names (comma separated, or use ; to add a country code, e.g. "
          "Paris,FR;London,GB):<br><input type='text' name='city' value='" +
          htmlEscape(city) + "'><br>";
  html += "Bus Stop IDs (comma separated):<br><input type='text' "
          "name='busStop' value='" +
          htmlEscape(busStop) + "'><br>";
  html += "TMB App ID:<br><input type='text' name='appId' value='" +
          htmlEscape(appId) + "'><br>";
  html += "TMB App Key:<br><input type='password' name='appKey' value='" +
          htmlEscape(appKey) + "'><br>";
  html += "OWM API Key (Optional, enables AQI and OWM forecasts):<br><input "
          "type='password' name='owmApiKey' value='" +
          htmlEscape(owmApiKey) + "'><br>";
  html += String("Settings &amp; OTA Password (") +
          (webPassword.length() > 0 ? "currently SET" : "currently NOT set") +
          "; user 'admin'; leave blank to keep):"
          "<br><input type='password' name='webPassword' value=''><br>";
  if (webPassword.length() > 0)
    html += "<label><input type='checkbox' name='clearWebPassword' "
            "style='width:auto'> Remove password</label><br>";
  html += "<br>";

  // Improvements
  html += "<h3>Lighting</h3>";

  // Brightness Sliders
  html += "Day Brightness (" + String(dayBrightness) + "%):<br>";
  html += "<input type='range' name='dayBrightness' min='1' max='100' value='" +
          String(dayBrightness) + "'><br>";

  html += "Night Brightness (" + String(nightBrightness) + "%):<br>";
  html +=
      "<input type='range' name='nightBrightness' min='1' max='100' value='" +
      String(nightBrightness) + "'><br><br>";

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
  html +=
      "Night End (Hour 0-23):<br><input type='number' name='nightEnd' value='" +
      String(nightEnd) + "'><br><br>";

  html += "<h3>Stock Ticker</h3>";
  html += "Symbols (comma split):<br><input type='text' name='stockSymbols' "
          "value='" +
          htmlEscape(stockSymbols) + "'><br><br>";

  html += "LED Brightness:<br><select name='ledBrightness'>";
  String b_opts[] = {"low", "medium", "high"};
  for (String o : b_opts) {
    String sel = (o == ledBrightness) ? " selected" : "";
    html += "<option value='" + o + "'" + sel + ">" + o + "</option>";
  }
  html += "</select><br><br>";

  html += "<input type='submit' value='Save & Reboot'></form>";
  html += "<p>IP: " + WiFi.localIP().toString() + " &middot; " + OTA_HOSTNAME +
          ".local</p>";

  // Device health (same figures as the "MON:" serial log lines)
  SystemMonitor::Stats st = SystemMonitor::read();
  char status[320];
  snprintf(status, sizeof(status),
           "<h3>Status</h3><p style='font-family:monospace;color:#aaa'>"
           "Firmware: %s %s<br>Uptime: %s<br>WiFi: %d dBm<br>"
           "Heap: %u free, %u min, %u largest block<br>"
           "Stack free: loop %d, net %d bytes</p>",
           __DATE__, __TIME__, SystemMonitor::formatUptime(st.uptimeS).c_str(),
           st.rssi, st.heapFree, st.heapMinFree, st.heapLargest,
           st.loopStackFree, st.netStackFree);
  html += status;
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void NetworkManager::handleSave() {
  if (!checkAuth())
    return;
  if (server.hasArg("city") && server.hasArg("busStop")) {
    city = server.arg("city");
    busStop = server.arg("busStop");
    appId = server.arg("appId");
    appKey = server.arg("appKey");
    owmApiKey = server.arg("owmApiKey");
    city.trim();
    busStop.trim();
    appId.trim();
    appKey.trim();
    owmApiKey.trim();

    if (server.hasArg("clearWebPassword"))
      webPassword = "";
    else if (server.arg("webPassword").length() > 0)
      webPassword = server.arg("webPassword");

    // New Params
    if (server.arg("timezone").length() > 0)
      timezone = server.arg("timezone");
    nightMode = server.hasArg("nightMode"); // Checkbox present = true
    nightStart = server.arg("nightStart").toInt();
    nightEnd = server.arg("nightEnd").toInt();

    // Brightness Params (with safe parsing)
    if (server.hasArg("dayBrightness"))
      dayBrightness = server.arg("dayBrightness").toInt();
    if (server.hasArg("nightBrightness"))
      nightBrightness = server.arg("nightBrightness").toInt();

    prefs.begin("weather_cfg", false);
    prefs.putString("city", city);
    prefs.putString("busStop", busStop);
    prefs.putString("app_id", appId);
    prefs.putString("app_key", appKey);
    prefs.putString("owmApiKey", owmApiKey);
    prefs.putString("webPassword", webPassword);

    prefs.putString("timezone", timezone);
    prefs.putBool("nightMode", nightMode);
    prefs.putInt("nightStart", nightStart);
    prefs.putInt("nightEnd", nightEnd);

    prefs.putInt("dayBrightness", dayBrightness);
    prefs.putInt("nightBrightness", nightBrightness);

    // Stocks
    stockSymbols = server.arg("stockSymbols");
    prefs.putString("stockSymbols", stockSymbols);

    // LED
    String led = server.arg("ledBrightness");
    if (led == "low" || led == "medium" || led == "high")
      ledBrightness = led;
    prefs.putString("ledBrightness", ledBrightness);

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

  char buf[128];
  snprintf(buf, sizeof(buf),
           "WiFi setup needed\n\nConnect your phone to WiFi\n\"%s\"\n\nthen "
           "open 192.168.4.1",
           myWiFiManager->getConfigPortalSSID().c_str());
  Serial.println(buf);
  if (statusCallback)
    statusCallback(buf); // Show the instructions on screen
}

void NetworkManager::setStatusCallback(void (*cb)(const char *msg)) {
  statusCallback = cb;
}

void NetworkManager::begin() {
  Serial.println("NETWORK: Begin...");
  prefs.begin("weather_cfg", false);
  city = prefs.getString("city", "Barcelona");
  busStop = prefs.getString("busStop", "2156");
  appId = prefs.getString("app_id", "");
  appKey = prefs.getString("app_key", "");

  // Load Improvements
  timezone = prefs.getString("timezone", "CET-1CEST,M3.5.0,M10.5.0/3");
  nightMode = prefs.getBool("nightMode", false);
  nightStart = prefs.getInt("nightStart", 22);
  nightEnd = prefs.getInt("nightEnd", 7);

  dayBrightness = prefs.getInt("dayBrightness", 100);    // Default 100%
  nightBrightness = prefs.getInt("nightBrightness", 10); // Default 10%

  // Load Stocks
  stockSymbols = prefs.getString("stockSymbols", "AAPL,BTC-USD,GRF.MC");
  ledBrightness = prefs.getString("ledBrightness", "medium");

  // Custom Keys
  owmApiKey = prefs.getString("owmApiKey", "");
  webPassword = prefs.getString("webPassword", "");

  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setAPCallback(configModeCallback); // Show when in AP mode

  // Custom Parameters
  // id/name, placeholder/prompt, default, length
  WiFiManagerParameter custom_city("city", "City Name", city.c_str(), 128);
  // 64 chars: room for up to 5 comma-separated stop IDs
  WiFiManagerParameter custom_busStop("busStop", "Bus Stop IDs",
                                      busStop.c_str(), 64);
  WiFiManagerParameter custom_appId("appId", "TMB App ID", appId.c_str(), 32);
  WiFiManagerParameter custom_appKey("appKey", "TMB App Key", appKey.c_str(),
                                     64);

  // Lets the password be set during first setup, before the device is
  // reachable on the home network. Blank keeps the current one.
  WiFiManagerParameter custom_webPassword(
      "webPassword", "Settings &amp; OTA Password (optional, user 'admin')", "",
      64, "type='password'");

  wm.addParameter(&custom_city);
  wm.addParameter(&custom_busStop);
  wm.addParameter(&custom_appId);
  wm.addParameter(&custom_appKey);
  wm.addParameter(&custom_webPassword);

  // Set timeout
  wm.setConfigPortalTimeout(180);

  Serial.println("NETWORK: Attempting AutoConnect...");
  if (!wm.autoConnect("WeatherClockAP")) {
    Serial.println("NETWORK: Failed to connect and hit timeout");
    ESP.restart();
  }
  Serial.println("NETWORK: WiFi Connected!");

  // Init NTP with the configured POSIX time zone
  configTzTime(timezone.c_str(), "pool.ntp.org");

  if (shouldSaveConfig) {
    Serial.println("NETWORK: Saving New Config...");
    city = custom_city.getValue();
    busStop = custom_busStop.getValue();
    appId = custom_appId.getValue();
    appKey = custom_appKey.getValue();

    Serial.printf("NETWORK: New City: %s, BusStop: %s\n", city.c_str(),
                  busStop.c_str());

    prefs.putString("city", city);
    prefs.putString("busStop", busStop);
    prefs.putString("app_id", appId);
    prefs.putString("app_key", appKey);

    String newPassword = custom_webPassword.getValue();
    if (newPassword.length() > 0) {
      webPassword = newPassword;
      prefs.putString("webPassword", webPassword);
    }
    // Other settings live in the web UI (http://weatherclock.local)
  }
  prefs.end();

  // Start Web Server
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(
      []() { server.send(404, "text/plain", "Not Found"); }); // Catch-all
  server.begin();
  Serial.println("NETWORK: Web Server Started.");

  setupOTA();

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

String NetworkManager::getAppId() { return appId; }

String NetworkManager::getAppKey() { return appKey; }

String NetworkManager::getTimezone() { return timezone; }
bool NetworkManager::getNightModeEnabled() { return nightMode; }
int NetworkManager::getNightStart() { return nightStart; }
int NetworkManager::getNightEnd() { return nightEnd; }
String NetworkManager::getStockSymbols() { return stockSymbols; }
