#include "BusService.h"
#include "GuiController.h"
#include "LedController.h"
#include "NetworkManager.h"
#include "StockService.h"
#include "TouchDrv.h"
#include "WeatherService.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <lvgl.h>

// Hardware Pins (Touch)
#define TOUCH_SDA 33
#define TOUCH_SCL 32
#define TOUCH_INT 21
#define TOUCH_RST 25

// --- GLOBAL STATE ---
WeatherData weatherData;
BusData busData;
SemaphoreHandle_t dataMutex;
volatile bool busDataUpdated = false;
volatile bool weatherDataUpdated = false;
volatile bool pendingBusRedraw = false;
bool pendingWeatherRedraw = false;
bool triggerBusFetch = false;
bool triggerWeatherFetch = false;
bool triggerStockFetch = true; // Force initial fetch on boot

// Timers
uint32_t lastWeatherUpdate = 0;
uint32_t lastBusUpdate = 0;
uint32_t lastStockUpdate = 0;

// Stock Data
std::vector<StockItem> stockData;
volatile bool stockDataUpdated = false;

// Variables defining current view
float lat = 0;
float lon = 0;

// --- TOUCH DRIVER (CST816S) ---
TouchDrv touch; // Instance

// Minimal driver adaptation for LVGL
void my_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  int16_t x, y;
  if (touch.read(&x, &y)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
    // Serial.printf("LVGL IN: x=%d, y=%d\n", x, y); // Removed debug
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// --- NETWORK TASK (Background) ---
void networkTask(void *parameter) {
  // Wait for mutex to be created
  while (dataMutex == NULL)
    vTaskDelay(10);

  // 1. Initial Connection
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    GuiController::showLoadingScreen("Connecting WiFi...");
    xSemaphoreGive(dataMutex);
  }

  NetworkManager::begin();

  // 2. Initial Data Fetch
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    GuiController::showLoadingScreen("Fetching Weather...");
    xSemaphoreGive(dataMutex);
  }

  String city = NetworkManager::getCity();
  String resolvedName;
  if (WeatherService::lookupCoordinates(city, lat, lon, resolvedName)) {
    weatherData.cityName = resolvedName;
  } else {
    weatherData.cityName = city;
  }

  // --- FIX: Initial Weather Fetch ---

  // 1. Initial City Setup
  std::vector<String> cities = NetworkManager::getCities();
  GuiController::setCityCount(cities.size());

  struct CityWeatherCache {
    String cityName;
    WeatherData data;
    uint32_t lastUpdate;
    bool hasData;
  };
  std::vector<CityWeatherCache> cityCaches(cities.size());
  for (size_t i = 0; i < cities.size(); i++) {
    cityCaches[i].cityName = cities[i];
    cityCaches[i].lastUpdate = 0;
    cityCaches[i].hasData = false;
  }

  // 2. Fetch Primary City Immediately (Index 0)
  Serial.printf("NETWORK: Fetching Primary City: %s\n", cities[0].c_str());
  WeatherData tempWeather;
  float pLat, pLon;
  String pResolved;

  bool primarySuccess = false;
  if (WeatherService::lookupCoordinates(cities[0], pLat, pLon, pResolved)) {
    if (WeatherService::updateWeather(tempWeather, pLat, pLon)) {
      tempWeather.cityName = (pResolved.length() > 0) ? pResolved : cities[0];

      cityCaches[0].data = tempWeather;
      cityCaches[0].hasData = true;
      cityCaches[0].lastUpdate = millis();

      xSemaphoreTake(dataMutex, portMAX_DELAY);
      weatherData = tempWeather;
      LedController::update(weatherData); // LED linked to Primary
      xSemaphoreGive(dataMutex);
      primarySuccess = true;
    }
  }

  if (!primarySuccess)
    Serial.println("NETWORK: Failed initial primary fetch.");
  // ----------------------------------

  // Initial Bus Setup
  std::vector<String> stopIds = NetworkManager::getBusStops();
  GuiController::setBusStopCount(stopIds.size());

  struct BusStopCache {
    String id;
    BusData data;
    uint32_t lastUpdate;
  };
  std::vector<BusStopCache> busCaches(stopIds.size());
  for (size_t i = 0; i < stopIds.size(); i++) {
    busCaches[i].id = stopIds[i];
    busCaches[i].lastUpdate = 0;
  }

  // Request transition to Main Screen
  pendingWeatherRedraw = true;

  // 3. Update Loop
  for (;;) {
    uint32_t now = millis();

    // --- WEATHER LOGIC (Multi-City) ---
    // 1. Check if we need to fetch ANY city
    int targetCityIndex = GuiController::getCityIndex();
    bool citySwitched = GuiController::hasCityChanged();

    // A. Always check Primary City (Index 0) for background update (every 10m)
    if (now - cityCaches[0].lastUpdate > 600000) {
      Serial.println("NETWORK: Updating Primary City (Background)...");
      WeatherData temp;
      float lat, lon;
      String res;
      if (WeatherService::lookupCoordinates(cityCaches[0].cityName, lat, lon,
                                            res)) {
        if (WeatherService::updateWeather(temp, lat, lon)) {
          temp.cityName = (res.length() > 0) ? res : cityCaches[0].cityName;
          cityCaches[0].data = temp;
          cityCaches[0].lastUpdate = now;
          cityCaches[0].hasData = true;

          // Update Global State (and LED)
          xSemaphoreTake(dataMutex, portMAX_DELAY);
          // If we are currently viewing primary city, push data to text
          if (targetCityIndex == 0) {
            weatherData = temp;
            weatherDataUpdated = true;
          }
          // ALWAYS update LED based on Primary
          LedController::update(temp);
          xSemaphoreGive(dataMutex);
        }
      }
    }

    // B. Check Active City (if not primary, or if explicit switch)
    if (targetCityIndex >= 0 && targetCityIndex < cityCaches.size()) {
      bool needFetch = false;

      if (citySwitched) {
        // If just switched, check cache
        if (!cityCaches[targetCityIndex].hasData ||
            (now - cityCaches[targetCityIndex].lastUpdate > 600000)) {
          needFetch = true;
        } else {
          // Cache is good!
          xSemaphoreTake(dataMutex, portMAX_DELAY);
          weatherData = cityCaches[targetCityIndex].data;
          weatherDataUpdated = true;
          xSemaphoreGive(dataMutex);
          GuiController::clearCityChanged();
        }
      } else if (!citySwitched && targetCityIndex != 0) {
        // If sitting on a secondary city, update it periodically (10m)
        if (now - cityCaches[targetCityIndex].lastUpdate > 600000) {
          needFetch = true;
        }
      }

      if (needFetch) {
        Serial.printf("NETWORK: Fetching City %d: %s\n", targetCityIndex,
                      cityCaches[targetCityIndex].cityName.c_str());
        WeatherData temp;
        float lat, lon;
        String res;
        if (WeatherService::lookupCoordinates(
                cityCaches[targetCityIndex].cityName, lat, lon, res)) {
          if (WeatherService::updateWeather(temp, lat, lon)) {
            temp.cityName =
                (res.length() > 0) ? res : cityCaches[targetCityIndex].cityName;
            cityCaches[targetCityIndex].data = temp;
            cityCaches[targetCityIndex].lastUpdate = now;
            cityCaches[targetCityIndex].hasData = true;

            xSemaphoreTake(dataMutex, portMAX_DELAY);
            weatherData = temp;
            weatherDataUpdated = true;
            xSemaphoreGive(dataMutex);
            GuiController::clearCityChanged();
          }
        }
      }
    }

    // 3. Update Bus (Logic: Active Stop Only, >60s old)
    int targetIndex = GuiController::getBusIndex();
    bool stationChanged = GuiController::hasBusStationChanged();

    // Check if we need to fetch
    bool needFetch = false;
    if (targetIndex >= 0 && targetIndex < busCaches.size()) {
      if (stationChanged) {
        // If switching stations, trigger logic.
        // If cache is fresh (<60s) AND valid, just show it. Otherwise fetch.
        if (now - busCaches[targetIndex].lastUpdate > 60000 ||
            busCaches[targetIndex].data.stopCode.isEmpty()) {
          needFetch = true;
        } else {
          // Cache is good! Just push to UI
          xSemaphoreTake(dataMutex, portMAX_DELAY);
          busData = busCaches[targetIndex].data;
          busDataUpdated = true;
          xSemaphoreGive(dataMutex);
          GuiController::clearBusStationChanged();
        }
      } else if (GuiController::isBusScreenActive()) {
        // Periodic update for active screen OR forced trigger
        if (now - busCaches[targetIndex].lastUpdate > 60000 ||
            triggerBusFetch) {
          needFetch = true;
          triggerBusFetch = false; // Clear the trigger
        }
      }
    }

    if (needFetch) {
      String stopId = busCaches[targetIndex].id;
      Serial.printf("NETWORK: Updating Bus Stop %s...\n", stopId.c_str());
      BusData tempBus;
      if (BusService::updateBusTimes(tempBus, stopId,
                                     NetworkManager::getAppId().c_str(),
                                     NetworkManager::getAppKey().c_str())) {
        busCaches[targetIndex].data = tempBus;
        busCaches[targetIndex].lastUpdate = now;

        xSemaphoreTake(dataMutex, portMAX_DELAY);
        busData = tempBus;
        busDataUpdated = true;
        xSemaphoreGive(dataMutex);

        GuiController::clearBusStationChanged();
      }
    }

    // 4. Update Stock (Every 5 mins)
    if (now - lastStockUpdate > 300000 || triggerStockFetch) {
      String syms = NetworkManager::getStockSymbols();

      Serial.printf("MAIN: Checking Stock Update... Syms='%s'\n", syms.c_str());

      if (syms.length() > 0) {
        Serial.println("NETWORK: Updating Stocks (Yahoo)...");
        std::vector<StockItem> items = StockService::getQuotes(syms);
        if (!items.empty()) {
          xSemaphoreTake(dataMutex, portMAX_DELAY);
          stockData = items;
          stockDataUpdated = true;
          xSemaphoreGive(dataMutex);
        }
      }
      lastStockUpdate = now;
      triggerStockFetch = false;
    }

    // Handle Web Requests
    NetworkManager::handleClient();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Logic to switch screens - REMOVED (Handled by GuiController internally)

// GUI Callbacks - REMOVED (Handled by GuiController internally)

// Time-Based Backlight Helper
void updateBacklight() {
  static uint32_t last_check = 0;
  static int current_pwm = -1;
  const int DAY_PWM = 255;
  const int NIGHT_PWM = 20;

  if (millis() - last_check > 5000) {
    last_check = millis();
    struct tm timeinfo;
    int target_pwm = DAY_PWM;

    // Only apply logic if Night Mode is enabled and time is valid
    if (NetworkManager::getNightModeEnabled() && getLocalTime(&timeinfo)) {
      int h = timeinfo.tm_hour;
      int start = NetworkManager::getNightStart();
      int end = NetworkManager::getNightEnd();

      bool isNight = false;
      if (start > end) {
        // Example: 22 to 07 (Crosses Midnight)
        if (h >= start || h < end)
          isNight = true;
      } else {
        // Example: 01 to 05 (Same day)
        if (h >= start && h < end)
          isNight = true;
      }

      if (isNight) {
        target_pwm = NIGHT_PWM;
      }
    }

    if (target_pwm != current_pwm) {
      // Serial.printf("Time Update -> Backlight PWM: %d\n", target_pwm);
      ledcWrite(0, target_pwm); // Pin 27
      current_pwm = target_pwm;
    }
  }
}

void setup() {
  Serial.begin(115200);
  dataMutex = xSemaphoreCreateMutex();

  // --- TOUCH FIRST (Avoid resetting peripherals later) ---
  touch.begin();

  // --- AUTO-BRIGHTNESS SETUP ---
  pinMode(34, INPUT);
  ledcSetup(0, 5000, 8);
  ledcAttachPin(27, 0);

  Serial.println("LEDC Backlight Configured.");

  // Initialize UI (Screen and LVGL)
  GuiController::init();
  LedController::begin(); // Initialize LED

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touch_read;
  lv_indev_drv_register(&indev_drv);

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    GuiController::showLoadingScreen("Starting Config...");
    xSemaphoreGive(dataMutex);
  }

  // Start Background Task (handles WiFi and Setup)
  xTaskCreatePinnedToCore(networkTask, "NetTask", 10240, NULL, 1, NULL, 0);
  Serial.println("SETUP: Core services started.");
}

void loop() {
  // --- THREAD-SAFE LVGL ---
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    GuiController::update(); // Handles loading screen queue + lv_timer_handler
    xSemaphoreGive(dataMutex);
  }

  // Check for redraw requests from background
  if (pendingWeatherRedraw) {
    pendingWeatherRedraw = false;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    GuiController::showWeatherScreen(weatherData, 0);
    xSemaphoreGive(dataMutex);
  }

  if (pendingBusRedraw) {
    pendingBusRedraw = false;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    GuiController::showBusScreen(busData, 0);
    xSemaphoreGive(dataMutex);
  }

  // lv_tick_inc is handled by LV_TICK_CUSTOM
  uint32_t now = millis();

  // Update Timers
  static uint32_t lastTimeUpdate = now;

  // 1. Check for background data updates
  if (busDataUpdated) {
    BusData localBus;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    localBus = busData;
    busDataUpdated = false;

    // Always update cache so it's ready for gesture
    GuiController::updateBusCache(localBus);

    // Check TRUE state from GUI
    bool shouldShow = GuiController::isBusScreenActive();
    xSemaphoreGive(dataMutex);

    if (shouldShow) {
      GuiController::showBusScreen(localBus, 0);
    }
  }

  if (stockDataUpdated) {
    std::vector<StockItem> localStock;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    localStock = stockData;
    stockDataUpdated = false;
    GuiController::updateStockCache(localStock);
    bool shouldShow = GuiController::isStockScreenActive();
    xSemaphoreGive(dataMutex);

    if (shouldShow) {
      GuiController::showStockScreen(localStock, 0);
    }
  }

  if (weatherDataUpdated) {
    WeatherData localWeather;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    localWeather = weatherData;
    weatherDataUpdated = false;
    bool shouldShow = !GuiController::isBusScreenActive();
    xSemaphoreGive(dataMutex);

    if (shouldShow) {
      // Serial.println("Redrawing Weather Screen (Update Ready)...");
      GuiController::showWeatherScreen(localWeather, 0);
    }
  }

  // 2. Trigger requests periodically
  if (GuiController::isBusScreenActive()) {
    // On bus screen: update bus frequently
    if (now - lastBusUpdate > 60000) { // Check every 60s
      // Serial.println("Triggering background Bus update...");
      triggerBusFetch = true;
      lastBusUpdate = now;
    }
  } else if (GuiController::isStockScreenActive()) {
    // On stock screen: update frequently
    if (now - lastStockUpdate > 60000) {
      triggerStockFetch = true;
      lastStockUpdate = now;
    }
  } else {
    // On weather screen: update clock and weather

    // Clock Update (Efficient)
    if (now - lastTimeUpdate > 10000) { // Update every 10s
      lastTimeUpdate = now;
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      GuiController::updateTime();
      xSemaphoreGive(dataMutex);

      // Update Backlight (Check Night Mode)
      updateBacklight();
    }

    // Weather Fetch Trigger
    if (now - lastWeatherUpdate > 600000) { // 10 mins
      // Serial.println("Triggering background Weather update...");
      triggerWeatherFetch = true;
      lastWeatherUpdate = now;
    }
  }

  // Detect App Switching (Force Update on Entry)
  static bool wasBusActive = false;
  bool isBus = GuiController::isBusScreenActive();
  if (isBus && !wasBusActive) {
    // Just entered Bus Screen
    Serial.println("MAIN: Switched to Bus App -> Forcing Update");
    triggerBusFetch = true;
  }
  wasBusActive = isBus;
}
