#include "BusService.h"
#include "DataManager.h" // New Manager
#include "GuiController.h"
#include "LedController.h"
#include "NetworkManager.h"
#include "TouchDrv.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>


// --- TOUCH DRIVER (CST816S) ---
TouchDrv touch;

void my_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  int16_t x, y;
  if (touch.read(&x, &y)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// Time-Based Backlight Helper (Could move to Ledger or DataManager, but fine
// here for now)
void updateBacklight() {
  static uint32_t last_check = 0;
  static int current_pwm = -1;
  const int DAY_PWM = 255;
  const int NIGHT_PWM = 20;

  if (millis() - last_check > 360000) {
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
        if (h >= start || h < end)
          isNight = true;
      } else {
        if (h >= start && h < end)
          isNight = true;
      }

      if (isNight)
        target_pwm = NIGHT_PWM;
    }

    if (target_pwm != current_pwm) {
      ledcWrite(0, target_pwm); // Pin 27
      current_pwm = target_pwm;
    }
  }
}

void setup() {
  Serial.begin(115200);

  // --- HARDWARE INIT ---
  touch.begin();

  pinMode(34, INPUT);
  ledcSetup(0, 5000, 8);
  ledcAttachPin(27, 0);
  Serial.println("LEDC Backlight Configured.");

  GuiController::init();
  LedController::begin();

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touch_read;
  lv_indev_drv_register(&indev_drv);

  // --- DATA MANAGER INIT ---
  // Starts the background task for WiFi and Data Fetching
  DataManager::begin();

  Serial.println("SETUP: Core services started.");
}

void loop() {
  // --- GUI UPDATE ---
  GuiController::update();

  // --- DATA SYNC ---
  // We poll DataManager for thread-safe updates

  // 1. Weather Update
  WeatherData wd;
  if (DataManager::getWeatherData(wd)) {
    // If we are getting the first update, show it
    GuiController::showWeatherScreen(wd, 0);
  }

  // 2. Bus Update
  BusData bd;
  if (DataManager::getBusData(bd)) {
    GuiController::updateBusCache(bd);
    if (GuiController::isBusScreenActive()) {
      GuiController::showBusScreen(bd, 0);
    }
  }

  // 3. Stock Update
  std::vector<StockItem> sd;
  if (DataManager::getStockData(sd)) {
    GuiController::updateStockCache(sd);
    if (GuiController::isStockScreenActive()) {
      GuiController::showStockScreen(sd, 0);
    }
  }

  // --- APP STATE TRIGGERS ---

  // Detect App Switching (Force Update on Entry)
  static bool wasBusActive = false;
  bool isBus = GuiController::isBusScreenActive();
  if (isBus && !wasBusActive) {
    Serial.println("MAIN: Switched to Bus App -> Forcing Update");
    DataManager::triggerBusUpdate();
  }
  wasBusActive = isBus;

  // Clock & Backlight
  static uint32_t lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate > 10000) {
    lastTimeUpdate = millis();
    GuiController::updateTime();
    updateBacklight();
  }
}
