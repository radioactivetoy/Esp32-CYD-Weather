#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t dataMutex;

class NetworkManager {
public:
  static void begin();
  static void reset();

  static void handleClient(); // New: Handle web requests

  static String getCity();
  static String getBusStop();
  static String getAppId();
  static String getAppKey();

  // Legacy method if used
  static bool isConnected() { return WiFi.status() == WL_CONNECTED; }

private:
  static Preferences prefs;
  static WebServer server; // New: Web Server instance
  static String city;
  static String busStop;
  static bool shouldSaveConfig;
  static void saveConfigCallback();
  static void configModeCallback(WiFiManager *myWiFiManager);

  // New: Web Handlers
  static void handleRoot();
  static void handleSave();
};
