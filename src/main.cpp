#include "BusService.h"
#include "DataManager.h" // New Manager
#include "GuiController.h"
#include "LedController.h"
#include "NetworkManager.h"
#include "SystemMonitor.h"
#include "TouchDrv.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

// --- BACKLIGHT / NIGHT MODE ---
// This TN panel loses most of its contrast when dimmed, so the backlight is
// only ever fully on or off. Night mode (optional, set in the web settings):
// the screen is off between the configured hours; a tap turns it on for
// WAKE_MS, and that first tap only wakes it (doesn't switch views or stops).
static const uint32_t WAKE_MS = 30000;
static bool screenOn = true; // tft.begin() switches the backlight on
static bool nightNow = false;
static uint32_t wakeUntil = 0;

static void setScreen(bool on) {
  if (on == screenOn)
    return;
  digitalWrite(TFT_BL, on ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
  screenOn = on;
  Serial.printf("BACKLIGHT: %s\n", on ? "on" : "off");
}

// Inside the night window (handles windows across midnight, e.g. 22 -> 7)?
static bool isNightTime() {
  struct tm t;
  if (!NetworkManager::getNightModeEnabled() || !getLocalTime(&t, 0))
    return false; // Disabled, or no valid time yet: keep the screen on
  int h = t.tm_hour;
  int start = NetworkManager::getNightStart();
  int end = NetworkManager::getNightEnd();
  if (start == end)
    return false;
  return (start > end) ? (h >= start || h < end) : (h >= start && h < end);
}

// Called every loop: cheap except for the time check every 10s.
static void updateBacklight() {
  static uint32_t lastCheck = 0;
  uint32_t now = millis();
  if (lastCheck == 0 || now - lastCheck >= 10000) {
    lastCheck = now ? now : 1;
    nightNow = isNightTime();
  }
  bool awake = (int32_t)(wakeUntil - now) > 0;
  setScreen(!nightNow || awake);
}

// --- TOUCH DRIVER (CST816S) ---
TouchDrv touch;

void my_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  static bool swallowing = false; // The touch that woke the screen
  int16_t x, y;
  bool pressed = touch.read(&x, &y);

  if (pressed) {
    if (!screenOn) {
      // Screen off at night: this touch only wakes it
      wakeUntil = millis() + WAKE_MS;
      setScreen(true);
      swallowing = true;
    } else if (nightNow) {
      wakeUntil = millis() + WAKE_MS; // Stay on while being used
    }
  }

  if (swallowing) {
    if (!pressed)
      swallowing = false; // Lifted: the next touch acts normally
    data->state = LV_INDEV_STATE_REL;
  } else if (pressed) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
  // Data refreshes wait while the screen is touched
  GuiController::setTouchActive(pressed);
}

void setup() {
  Serial.begin(115200);
  Serial.printf("BOOT: last reset: %s\n", SystemMonitor::resetReason());

  // --- HARDWARE INIT ---
  touch.begin();

  // The backlight (TFT_BL) is switched on by tft.begin() inside init() and
  // then only ever fully on or off (see updateBacklight)
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

  // Reused across iterations to avoid reallocating large structs every pass
  static WeatherData wd;
  static BusData bd;
  static std::vector<StockItem> sd;

  // GuiController decides per update: full rebuild only when something
  // displayed changed, otherwise in-place (status dot, bus ETAs).
  if (DataManager::getWeatherData(wd))
    GuiController::applyWeatherData(wd);
  if (DataManager::getBusData(bd))
    GuiController::applyBusData(bd);
  if (DataManager::getStockData(sd))
    GuiController::applyStockData(sd);

  // A fetch started/finished for the visible screen: yellow dot on/off
  if (DataManager::getWeatherStatusChanged() &&
      GuiController::isWeatherScreenActive())
    GuiController::onStatusChanged();
  if (DataManager::getBusStatusChanged() && GuiController::isBusScreenActive())
    GuiController::onStatusChanged();
  // Stocks have no status signal; watch the flag (yellow dot while fetching)
  static bool wasStockUpdating = false;
  bool stockUpdating = DataManager::isStockUpdating();
  if (stockUpdating != wasStockUpdating) {
    wasStockUpdating = stockUpdating;
    if (GuiController::isStockScreenActive())
      GuiController::refreshStatusDot();
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

  // Clock
  static uint32_t lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate > 10000) {
    lastTimeUpdate = millis();
    GuiController::updateTime();
  }

  // Night mode: every pass, so a tap-to-wake timeout ends on time
  updateBacklight();

  // Heap / stack health line every minute on the serial log
  SystemMonitor::logPeriodic();

  // Yield instead of spinning at 100% CPU; LVGL only needs a few ms cadence.
  delay(5);
}
