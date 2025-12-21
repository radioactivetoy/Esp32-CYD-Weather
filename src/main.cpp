#include "BusService.h"
#include "GuiController.h"
#include "NetworkManager.h"
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
volatile bool triggerBusFetch = false;
volatile bool triggerWeatherFetch = false;
static uint32_t lastBusUpdate = 0;
static uint32_t lastWeatherUpdate = 0;

// Variables defining current view
volatile bool pendingWeatherRedraw = false;
volatile bool pendingBusRedraw = false;
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
    Serial.printf("LVGL IN: x=%d, y=%d\n", x, y); // Debug input to LVGL
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
  Serial.println("NETWORK: Fetching Initial Weather...");
  WeatherData tempWeather;
  if (WeatherService::updateWeather(tempWeather, lat, lon)) {
    tempWeather.cityName = (resolvedName.length() > 0) ? resolvedName : city;

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    weatherData = tempWeather;
    xSemaphoreGive(dataMutex);
    Serial.println("NETWORK: Initial Weather Fetched Successfully.");
  } else {
    Serial.println("NETWORK: Failed to fetch initial weather.");
  }
  // ----------------------------------

  // Initial Fetch Bus
  String stopId = NetworkManager::getBusStop();
  if (BusService::updateBusTimes(busData, stopId,
                                 NetworkManager::getAppId().c_str(),
                                 NetworkManager::getAppKey().c_str())) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    busDataUpdated = true;
    xSemaphoreGive(dataMutex);
  }

  // Request transition to Main Screen
  pendingWeatherRedraw = true;

  // 3. Update Loop
  for (;;) {
    uint32_t now = millis();

    // 2. Update Weather (Every 10 mins)
    if (now - lastWeatherUpdate > 600000 || triggerWeatherFetch) {
      Serial.println("NETWORK: Updating Weather...");
      WeatherData temp;
      String currentCity = NetworkManager::getCity();
      float lat, lon;
      String resolved;

      // Resolve coords for current city
      if (WeatherService::lookupCoordinates(currentCity, lat, lon, resolved)) {
        if (WeatherService::updateWeather(temp, lat, lon)) {
          temp.cityName = (resolved.length() > 0) ? resolved : currentCity;

          xSemaphoreTake(dataMutex, portMAX_DELAY);
          weatherData = temp;
          weatherDataUpdated = true;
          xSemaphoreGive(dataMutex);
        }
      }
      lastWeatherUpdate = now;
      triggerWeatherFetch = false;
    }

    // 3. Update Bus (Every 60s)
    // 3. Update Bus (Every 60s)
    if (now - lastBusUpdate > 60000 || triggerBusFetch) {
      Serial.println("NETWORK: Updating Bus...");
      BusData tempBus;
      // Use configured bus stop
      if (BusService::updateBusTimes(tempBus, NetworkManager::getBusStop(),
                                     NetworkManager::getAppId().c_str(),
                                     NetworkManager::getAppKey().c_str())) {
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        busData = tempBus;
        busDataUpdated = true;
        xSemaphoreGive(dataMutex);
      }
      lastBusUpdate = now; // Update timestamp
      triggerBusFetch = false;
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

    if (getLocalTime(&timeinfo)) {
      int h = timeinfo.tm_hour;
      if (h >= 22 || h < 7) {
        target_pwm = NIGHT_PWM;
      }
    }

    if (target_pwm != current_pwm) {
      Serial.printf("Time Update -> Backlight PWM: %d\n", target_pwm);
      ledcWrite(0, target_pwm); // Pin 27
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

  if (weatherDataUpdated) {
    WeatherData localWeather;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    localWeather = weatherData;
    weatherDataUpdated = false;
    bool shouldShow = !GuiController::isBusScreenActive();
    xSemaphoreGive(dataMutex);

    if (shouldShow) {
      Serial.println("Redrawing Weather Screen (Update Ready)...");
      GuiController::showWeatherScreen(localWeather, 0);
    }
  }

  // 2. Trigger requests periodically
  if (GuiController::isBusScreenActive()) {
    // On bus screen: update bus frequently
    if (now - lastBusUpdate > 60000) { // Check every 60s
      Serial.println("Triggering background Bus update...");
      triggerBusFetch = true;
      lastBusUpdate = now;
    }
  } else {
    // On weather screen: update clock and weather

    // Clock Update (Efficient)
    if (now - lastTimeUpdate > 10000) { // Update every 10s
      lastTimeUpdate = now;
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      GuiController::updateTime();
      xSemaphoreGive(dataMutex);
    }

    // Weather Fetch Trigger
    if (now - lastWeatherUpdate > 600000) { // 10 mins
      Serial.println("Triggering background Weather update...");
      triggerWeatherFetch = true;
      lastWeatherUpdate = now;
    }
  }
}
