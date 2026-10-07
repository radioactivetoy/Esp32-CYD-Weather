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

// --- Settings page ---------------------------------------------------------

// Same look as the device: black, white values, light grey labels, hairline
// section dividers, one accent colour (rain blue) for controls.
static const char PAGE_HEAD[] = R"HTML(<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Weather Clock</title><style>
:root{color-scheme:dark}
body{margin:0;background:#000;color:#fff;font:16px/1.45 -apple-system,system-ui,"Segoe UI",Roboto,sans-serif}
main{max-width:480px;margin:0 auto;padding:16px 16px 104px}
h1{font-size:24px;font-weight:600;margin:8px 0 2px}
.sub{margin:0;color:#c8c8c8;font-size:14px}
section{border-top:1px solid #666;margin-top:22px;padding-top:12px}
h2{font-size:13px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;color:#c8c8c8;margin:0 0 4px}
label{display:block;margin:14px 0 6px;color:#c8c8c8;font-size:14px}
.hint{color:#9a9a9a;font-size:13px;margin-top:6px}
input[type=text],input[type=password],input[type=number],select{width:100%;box-sizing:border-box;background:#111;color:#fff;border:1px solid #555;border-radius:10px;padding:11px 12px;font:inherit}
input:focus,select:focus{outline:none;border-color:#44bbff}
input[type=range]{width:100%;accent-color:#44bbff;margin:4px 0}
.row{display:flex;gap:12px}.row>div{flex:1;min-width:0}
.check{display:flex;align-items:center;gap:10px;margin-top:14px;color:#fff;font-size:16px}
.check input{width:20px;height:20px;accent-color:#44bbff;margin:0}
.val{float:right;color:#fff}
dl{display:grid;grid-template-columns:auto 1fr;gap:6px 16px;margin:10px 0 0;font-size:14px}
dt{color:#c8c8c8}dd{margin:0;font-variant-numeric:tabular-nums}
.bar{position:fixed;left:0;right:0;bottom:0;background:#000;border-top:1px solid #666;padding:12px 16px}
.bar button{display:block;width:100%;max-width:480px;margin:0 auto;padding:14px;border:0;border-radius:12px;background:#44bbff;color:#000;font:600 16px system-ui,sans-serif}
.msg{text-align:center;padding-top:30vh}
</style></head><body><main>
)HTML";

static const char PAGE_TAIL[] = R"HTML(</main>
<div class="bar"><button type="submit" form="f">Save and restart</button></div>
<script>
document.querySelectorAll('input[type=range]').forEach(function(r){
  var o=document.getElementById(r.name+'Val');
  var u=function(){o.textContent=r.value+'%';};
  r.addEventListener('input',u);u();
});
</script></body></html>)HTML";

static const struct {
  const char *name;
  const char *val;
} TIMEZONES[] = {
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
    {"UTC", "GMT0"},
};

// Sends the page in ~1KB chunks instead of building it as one big String
// on the network task's heap.
class ChunkedPage {
public:
  explicit ChunkedPage(WebServer &s) : server(s) {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=utf-8", "");
    buf.reserve(1280);
  }
  void add(const char *s) {
    buf += s;
    flushIfFull();
  }
  void add(const String &s) {
    buf += s;
    flushIfFull();
  }
  void end() {
    if (buf.length())
      server.sendContent(buf);
    server.sendContent(""); // Terminating chunk
  }

private:
  void flushIfFull() {
    if (buf.length() >= 1024) {
      server.sendContent(buf);
      buf = "";
    }
  }
  WebServer &server;
  String buf;
};

void NetworkManager::handleRoot() {
  if (!checkAuth())
    return;
  using NetUtils::htmlEscape;

  ChunkedPage page(server);
  page.add(PAGE_HEAD);

  page.add("<h1>Weather Clock</h1><p class=\"sub\">");
  page.add(WiFi.localIP().toString());
  page.add(" &middot; ");
  page.add(OTA_HOSTNAME);
  page.add(".local</p><form id=\"f\" method=\"post\" action=\"/save\">");

  auto section = [&](const char *title) {
    page.add("<section><h2>");
    page.add(title);
    page.add("</h2>");
  };
  auto hint = [&](const char *text) {
    page.add("<div class=\"hint\">");
    page.add(text);
    page.add("</div>");
  };
  // Text-like input; the value is escaped for the double-quoted attribute
  auto input = [&](const char *label, const char *name, const String &value,
                   const char *type, const char *extra) {
    page.add("<label for=\"");
    page.add(name);
    page.add("\">");
    page.add(label);
    page.add("</label><input id=\"");
    page.add(name);
    page.add("\" name=\"");
    page.add(name);
    page.add("\" type=\"");
    page.add(type);
    page.add("\" value=\"");
    page.add(htmlEscape(value));
    page.add("\" autocomplete=\"off\" autocapitalize=\"off\" "
             "spellcheck=\"false\" ");
    page.add(extra);
    page.add(">");
  };
  auto slider = [&](const char *label, const char *name, int value) {
    page.add("<label>");
    page.add(label);
    page.add("<span class=\"val\" id=\"");
    page.add(name);
    page.add("Val\"></span></label><input type=\"range\" min=\"1\" "
             "max=\"100\" name=\"");
    page.add(name);
    page.add("\" value=\"");
    page.add(String(value));
    page.add("\">");
  };

  section("Locations");
  input("Cities", "city", city, "text", "");
  hint("Up to 5, comma separated. To add a country code, separate with ; "
       "instead: Paris,FR;London,GB");
  input("Bus stops", "busStop", busStop, "text", "inputmode=\"numeric\"");
  hint("TMB stop codes, up to 5, comma separated. Tap the bus screen to "
       "switch.");
  page.add("</section>");

  section("Stocks");
  input("Symbols", "stockSymbols", stockSymbols, "text", "");
  hint("Yahoo Finance symbols, comma separated: AAPL,BTC-USD,GRF.MC");
  page.add("</section>");

  section("Display");
  page.add("<label for=\"timezone\">Time zone</label>"
           "<select id=\"timezone\" name=\"timezone\">");
  for (const auto &tz : TIMEZONES) {
    page.add("<option value=\"");
    page.add(tz.val);
    page.add(timezone == tz.val ? "\" selected>" : "\">");
    page.add(tz.name);
    page.add("</option>");
  }
  page.add("</select><label for=\"ledBrightness\">Status LED</label>"
           "<select id=\"ledBrightness\" name=\"ledBrightness\">");
  static const char *ledValues[] = {"low", "medium", "high"};
  static const char *ledNames[] = {"Low", "Medium", "High"};
  for (int i = 0; i < 3; i++) {
    page.add("<option value=\"");
    page.add(ledValues[i]);
    page.add(ledBrightness == ledValues[i] ? "\" selected>" : "\">");
    page.add(ledNames[i]);
    page.add("</option>");
  }
  page.add("</select>");
  hint("Red: raining now. Orange: rain within ~2 h. Blue: rain later. "
       "Green: dry.");
  page.add("</section>");

  section("Night mode");
  page.add("<label class=\"check\"><input type=\"checkbox\" "
           "name=\"nightMode\"");
  page.add(nightMode ? " checked" : "");
  page.add(">Dim the screen at night</label><div class=\"row\"><div>");
  input("From (hour)", "nightStart", String(nightStart), "number",
        "min=\"0\" max=\"23\" inputmode=\"numeric\"");
  page.add("</div><div>");
  input("Until (hour)", "nightEnd", String(nightEnd), "number",
        "min=\"0\" max=\"23\" inputmode=\"numeric\"");
  page.add("</div></div>");
  slider("Day brightness", "dayBrightness", dayBrightness);
  slider("Night brightness", "nightBrightness", nightBrightness);
  page.add("</section>");

  section("API keys");
  input("TMB App ID", "appId", appId, "text", "");
  input("TMB App Key", "appKey", appKey, "password", "");
  hint("Free at developer.tmb.cat. Needed for bus times.");
  input("OpenWeatherMap key", "owmApiKey", owmApiKey, "password", "");
  hint("Optional. Adds air quality and OWM forecasts; without it weather "
       "comes from Open-Meteo.");
  page.add("</section>");

  section("Security");
  input("Password", "webPassword", "", "password",
        "placeholder=\"Leave blank to keep\"");
  hint(webPassword.length() > 0
           ? "Set. Protects this page (user admin) and OTA updates."
           : "Not set: anyone on your network can change settings or "
             "update the firmware.");
  if (webPassword.length() > 0)
    page.add("<label class=\"check\"><input type=\"checkbox\" "
             "name=\"clearWebPassword\">Remove password</label>");
  page.add("</section></form>");

  // Device health (same figures as the "MON:" serial log lines)
  SystemMonitor::Stats st = SystemMonitor::read();
  char status[420];
  snprintf(status, sizeof(status),
           "<section><h2>Status</h2><dl>"
           "<dt>Firmware</dt><dd>%s %s</dd>"
           "<dt>Uptime</dt><dd>%s</dd>"
           "<dt>WiFi</dt><dd>%d dBm</dd>"
           "<dt>Heap</dt><dd>%u KB free, %u KB min, %u KB largest</dd>"
           "<dt>Stack free</dt><dd>loop %d, net %d bytes</dd>"
           "</dl></section>",
           __DATE__, __TIME__, SystemMonitor::formatUptime(st.uptimeS).c_str(),
           st.rssi, st.heapFree / 1024, st.heapMinFree / 1024,
           st.heapLargest / 1024, st.loopStackFree, st.netStackFree);
  page.add(status);

  page.add(PAGE_TAIL);
  page.end();
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

    // Same style as the settings page; reloads it once the device is back
    String html = PAGE_HEAD;
    html.replace("<title>", "<meta http-equiv=\"refresh\" content=\"10;url=/\">"
                            "<title>");
    html += "<div class=\"msg\"><h1>Saved</h1><p class=\"sub\">Restarting. "
            "This page reloads in a few seconds.</p></div></main></body></html>";
    server.send(200, "text/html; charset=utf-8", html);
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
