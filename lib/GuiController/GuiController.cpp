#include "GuiController.h"
#include "BusService.h"
#include "NetworkManager.h"
#include "WeatherService.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cstdio>
#include <lvgl.h>
#include <WiFi.h>

// --- SCREEN DRIVER SETUP ---
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 320;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 60];
TFT_eSPI tft = TFT_eSPI();

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                   lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

// --- STATE VARIABLES ---
GuiController::AppMode GuiController::currentApp = GuiController::APP_WEATHER;

// Bus State
std::atomic<int> GuiController::currentBusIndex{0};
std::atomic<int> GuiController::busStopCount{1};
std::atomic<bool> GuiController::busStationChanged{false};
int GuiController::getBusIndex() { return currentBusIndex; }
void GuiController::setBusStopCount(int count) { busStopCount = count; }
bool GuiController::hasBusStationChanged() { return busStationChanged; }
void GuiController::clearBusStationChanged() { busStationChanged = false; }

// City State
std::atomic<int> GuiController::currentCityIndex{0};
std::atomic<int> GuiController::cityCount{1};
std::atomic<bool> GuiController::cityChanged{false};
int GuiController::getCityIndex() { return currentCityIndex; }
void GuiController::setCityCount(int count) { cityCount = count; }
bool GuiController::hasCityChanged() { return cityChanged; }
void GuiController::clearCityChanged() { cityChanged = false; }

bool GuiController::isBusScreenActive() { return currentApp == APP_BUS; }
bool GuiController::isStockScreenActive() { return currentApp == APP_STOCK; }
bool GuiController::isWeatherScreenActive() { return currentApp == APP_WEATHER; }
void GuiController::updateWeatherCache(const WeatherData &data) { cachedWeather = data; }
void GuiController::updateBusCache(const BusData &data) { cachedBus = data; }
void GuiController::updateStockCache(const std::vector<StockItem> &data) {
  cachedStock = data;
}

// Queue & Cache
String GuiController::pendingMsg = "";
volatile bool GuiController::needsUpdate = false;
GuiController::PendingScreen GuiController::pendingScreenChange = GuiController::SCREEN_NONE;
int GuiController::pendingScreenAnim = 0;
uint32_t GuiController::screenAnimUntil = 0;
int GuiController::pendingCitySwipeAnim = 0;
std::atomic<bool> GuiController::longPressTriggered{false};
SemaphoreHandle_t GuiController::guiMutex = NULL;
WeatherData GuiController::cachedWeather;
BusData GuiController::cachedBus;
std::vector<StockItem> GuiController::cachedStock;

// Local Controller State
static lv_obj_t *activeTimeLabel = NULL;
static int forecastMode = 0; // 0: Current, 1: Hourly, 2: Daily, 3: Chart
static uint32_t lastGestureTime = 0; // Used to suppress tap after swipe

// Custom Font
LV_FONT_DECLARE(font_intl_16);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);

void GuiController::init() {
  guiMutex = xSemaphoreCreateMutex();
  lv_init();
  tft.begin();
  tft.setRotation(0);
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 30);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  Serial.println("GuiController: LVGL initialized. Standard fonts linked.");
}

void GuiController::showLoadingScreen(const char *msg) {
  if (xSemaphoreTake(guiMutex, portMAX_DELAY) == pdTRUE) {
    if (msg)
      pendingMsg = String(msg);
    else
      pendingMsg = "Loading...";
    needsUpdate = true;
    xSemaphoreGive(guiMutex);
  }
}

void GuiController::update() {
  if (needsUpdate) {
    if (xSemaphoreTake(guiMutex, 5) == pdTRUE) {
      if (needsUpdate) { // Double check inside lock
        needsUpdate = false;
        drawLoadingScreen(pendingMsg.c_str());
      }
      xSemaphoreGive(guiMutex);
    }
  }
  lv_timer_handler();
  // Apply any screen transition requested from within event callbacks.
  // Must run AFTER lv_timer_handler() to avoid re-entrant lv_scr_load_anim calls
  // which corrupt LVGL's internal event dispatch and cause LoadProhibited crashes.
  applyPendingScreenChange();
}

void GuiController::applyPendingScreenChange() {
  if (pendingScreenChange == SCREEN_NONE)
    return;
  // Wait for any ongoing screen transition animation to finish before
  // starting another. Calling lv_scr_load_anim while an anim is active
  // corrupts LVGL's internal screen/event state and causes LoadProhibited.
  if (millis() < screenAnimUntil)
    return;
  PendingScreen toShow = pendingScreenChange;
  int anim = pendingScreenAnim;
  pendingScreenChange = SCREEN_NONE;
  if (anim != 0)
    screenAnimUntil = millis() + 350; // 300ms anim + 50ms buffer
  switch (toShow) {
  case SCREEN_WEATHER:
    showWeatherScreen(cachedWeather, anim);
    break;
  case SCREEN_BUS:
    showBusScreen(cachedBus, anim);
    break;
  case SCREEN_STOCK:
    showStockScreen(cachedStock, anim);
    break;
  default:
    break;
  }
}

void GuiController::requestRefresh() {
  // Only set pending if nothing else is already queued (don't overwrite a
  // swipe animation with a background data refresh).
  if (pendingScreenChange != SCREEN_NONE)
    return;
  switch (currentApp) {
  case APP_WEATHER:
    pendingScreenChange = SCREEN_WEATHER;
    pendingScreenAnim = pendingCitySwipeAnim;
    pendingCitySwipeAnim = 0; // consume — subsequent refreshes (e.g. yellow dot) use anim=0
    break;
  case APP_BUS:
    pendingScreenChange = SCREEN_BUS;
    pendingScreenAnim = 0;
    break;
  case APP_STOCK:
    pendingScreenChange = SCREEN_STOCK;
    pendingScreenAnim = 0;
    break;
  }
}

void GuiController::drawLoadingScreen(const char *msg) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  activeTimeLabel = NULL;

  lv_obj_set_style_bg_color(scr, lv_color_hex(0x001830), 0);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Weather Station");
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -30);
  lv_obj_set_style_text_color(title, lv_color_hex(0x00CCFF), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_text(label, msg ? msg : "Starting...");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 15);
  lv_obj_set_style_text_color(label, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
}

void GuiController::handle(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    lv_timer_handler();
    delay(5);
  }
}

String GuiController::sanitize(const String &text) {
  // Replace multi-byte UTF-8 sequences with single ASCII chars
  // Return by value (String) is thread-safe (stack memory)

  String buf = text;

  // c-cedilla
  buf.replace("\xC3\xA7", "c"); // ç
  buf.replace("\xC3\x87", "C"); // Ç

  // n-tilde
  buf.replace("\xC3\xB1", "n"); // ñ
  buf.replace("\xC3\x91", "N"); // Ñ

  // a-grave/acute
  buf.replace("\xC3\xA0", "a"); // à
  buf.replace("\xC3\xA1", "a"); // á
  buf.replace("\xC3\x80", "A"); // À
  buf.replace("\xC3\x81", "A"); // Á

  // e-grave/acute
  buf.replace("\xC3\xA8", "e"); // è
  buf.replace("\xC3\xA9", "e"); // é
  buf.replace("\xC3\x88", "E"); // È
  buf.replace("\xC3\x89", "E"); // É

  // i-acute/dieresis
  buf.replace("\xC3\xAD", "i"); // í
  buf.replace("\xC3\xAF", "i"); // ï
  buf.replace("\xC3\x8D", "I"); // Í
  buf.replace("\xC3\x8F", "I"); // Ï

  // o-grave/acute
  buf.replace("\xC3\xB2", "o"); // ò
  buf.replace("\xC3\xB3", "o"); // ó
  buf.replace("\xC3\x92", "O"); // Ò
  buf.replace("\xC3\x93", "O"); // Ó

  // u-acute/dieresis
  buf.replace("\xC3\xBA", "u"); // ú
  buf.replace("\xC3\xBC", "u"); // ü
  buf.replace("\xC3\x9A", "U"); // Ú
  buf.replace("\xC3\x9C", "U"); // Ü

  return buf;
}

// --- DELEGATED VIEW METHODS ---

void GuiController::showWeatherScreen(const WeatherData &data, int anim) {
  int32_t heapBefore = ESP.getFreeHeap();
  Serial.printf("GUI: Show Weather (Before Heap: %d)\n", heapBefore);

  activeTimeLabel = NULL; // CRITICAL: Reset pointer before transition
  if (&data != &cachedWeather)
    cachedWeather = data;
  WeatherView::show(data, anim, forecastMode);

  int32_t heapAfter = ESP.getFreeHeap();
  Serial.printf("GUI: Show Weather (After Heap: %d, delta: %d)\n", heapAfter,
                heapAfter - heapBefore);
}

void GuiController::showBusScreen(const BusData &data, int anim) {
  int32_t heapBefore = ESP.getFreeHeap();
  Serial.printf("GUI: Show Bus (Before Heap: %d)\n", heapBefore);

  activeTimeLabel = NULL;
  if (&data != &cachedBus)
    cachedBus = data;
  BusView::show(data, anim);

  int32_t heapAfter = ESP.getFreeHeap();
  Serial.printf("GUI: Show Bus (After Heap: %d, delta: %d)\n", heapAfter,
                heapAfter - heapBefore);
}

void GuiController::showStockScreen(const std::vector<StockItem> &data,
                                    int anim) {
  int32_t heapBefore = ESP.getFreeHeap();
  Serial.printf("GUI: Show Stock (Before Heap: %d)\n", heapBefore);

  activeTimeLabel = NULL; // CRITICAL: Reset pointer before transition
  if (&data != &cachedStock)
    cachedStock = data;
  StockView::show(data, anim);

  int32_t heapAfter = ESP.getFreeHeap();
  Serial.printf("GUI: Show Stock (After Heap: %d, delta: %d)\n", heapAfter,
                heapAfter - heapBefore);
}

// --- CONTROLLER LOGIC ---

void GuiController::setActiveTimeLabel(lv_obj_t *label) {
  activeTimeLabel = label;
}

void GuiController::updateTime() {
  if (activeTimeLabel == NULL)
    return;
  // Previously checked lv_obj_is_valid, but that is unsafe on freed pointers.
  // We rely on STRICT NULL management now.

  static uint32_t lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate < 1000)
    return; // Throttle to 1s
  lastTimeUpdate = millis();

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    char timeStr[32];
    // Simple format for generic update.
    // Ideally Views should format it, but updating generic label text is fine.
    // Bus/Stock use HH:MM. Weather uses HH:MM.
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    lv_label_set_text(activeTimeLabel, timeStr);
  }
}

static bool isDirUp(lv_dir_t dir) {
  return dir == LV_DIR_TOP;
}
static bool isDirDown(lv_dir_t dir) {
  return dir == LV_DIR_BOTTOM;
}
static bool isDirLeft(lv_dir_t dir) {
  return dir == LV_DIR_LEFT;
}
static bool isDirRight(lv_dir_t dir) {
  return dir == LV_DIR_RIGHT;
}

void GuiController::handleSwipe(int16_t dx, int16_t dy) {
  if (abs(dx) > abs(dy) && abs(dx) > 40) {
    if (dx > 0) {
      // right swipe → previous city
      if (currentApp == APP_WEATHER && cityCount > 1) {
        currentCityIndex = (currentCityIndex - 1 + cityCount) % cityCount;
        cityChanged = true;
        pendingCitySwipeAnim = -1; // new screen slides in from left
      }
    } else {
      // left swipe → next city
      if (currentApp == APP_WEATHER && cityCount > 1) {
        currentCityIndex = (currentCityIndex + 1) % cityCount;
        cityChanged = true;
        pendingCitySwipeAnim = 1; // new screen slides in from right
      }
    }
    return;
  }

  if (abs(dy) > abs(dx) && abs(dy) > 40) {
    if (dy < 0) {
      if (currentApp == APP_WEATHER) {
        pendingScreenChange = SCREEN_STOCK; pendingScreenAnim = -2;
      } else if (currentApp == APP_STOCK) {
        pendingScreenChange = SCREEN_BUS;   pendingScreenAnim = -2;
      } else if (currentApp == APP_BUS) {
        pendingScreenChange = SCREEN_WEATHER; pendingScreenAnim = -2;
      }
    } else {
      if (currentApp == APP_WEATHER) {
        pendingScreenChange = SCREEN_BUS;     pendingScreenAnim = 2;
      } else if (currentApp == APP_BUS) {
        pendingScreenChange = SCREEN_STOCK;   pendingScreenAnim = 2;
      } else if (currentApp == APP_STOCK) {
        pendingScreenChange = SCREEN_WEATHER; pendingScreenAnim = 2;
      }
    }
  }
  lastGestureTime = millis();
}

uint32_t GuiController::getLastGestureTime() {
  return lastGestureTime;
}

void GuiController::handleGesture(lv_event_t *e) {
  static uint32_t lastGestureMs = 0;
  uint32_t now = millis();
  if (now - lastGestureMs < 300) {
    return; // Debounce gesture nav to avoid rapid flick/bounce
  }
  lastGestureMs = now;

  lv_indev_t *indev = lv_indev_get_act();
  if (indev == NULL)
    return;
  lv_dir_t dir = lv_indev_get_gesture_dir(indev);

  Serial.printf("GESTURE: dir=%d, app=%d\n", dir, currentApp);
  lastGestureTime = millis();

  switch (dir) {
  case LV_DIR_LEFT:
    handleSwipe(-100, 0); // keep semantics for left/right touch
    break;
  case LV_DIR_RIGHT:
    handleSwipe(100, 0);
    break;
  case LV_DIR_TOP:
    handleSwipe(0, -100);
    break;
  case LV_DIR_BOTTOM:
    handleSwipe(0, 100);
    break;
  default:
    // no action for none
    break;
  }
}

void GuiController::handleScreenClick(lv_event_t *e) {
  // Fix: Ignore click if a gesture was detected or just happened.
  lv_indev_t *indev = lv_indev_get_act();
  if (indev == NULL)
    return;
  if (lv_indev_get_gesture_dir(indev) != LV_DIR_NONE)
    return;

  if (millis() - lastGestureTime < 500)
    return; // Prevent a swipe from triggering a click

  // Debounce to prevent rapid clicks causing OOM
  static uint32_t lastClickTime = 0;
  if (millis() - lastClickTime < 350) {
    return;
  }
  lastClickTime = millis();

  if (currentApp == APP_WEATHER) {
    forecastMode = (forecastMode + 1) % 3; // 0:Current 1:Hourly 2:7Days
    pendingScreenChange = SCREEN_WEATHER;
    pendingScreenAnim = 3; // fade transition for forecast mode cycle
  } else if (currentApp == APP_BUS) {
    // Switch Station on Tap
    if (busStopCount > 1) {
      currentBusIndex = (currentBusIndex + 1) % busStopCount;
      busStationChanged = true;
    }
  }
}

void GuiController::handleLongPress(lv_event_t *e) {
  longPressTriggered = true; // main.cpp polls this and calls the right trigger
}

void GuiController::showInfoOverlay(lv_event_t *e) {
  lv_event_stop_bubbling(e);

  lv_obj_t *overlay = lv_obj_create(lv_scr_act());
  lv_obj_set_size(overlay, 210, 198);
  lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(overlay, lv_color_hex(0x111122), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
  lv_obj_set_style_radius(overlay, 12, 0);
  lv_obj_set_style_border_color(overlay, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_style_border_width(overlay, 1, 0);
  lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(overlay, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(overlay, 10, 0);
  lv_obj_set_style_pad_row(overlay, 4, 0);
  lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(overlay, [](lv_event_t *ev) {
    lv_obj_del_async(lv_event_get_target(ev));
  }, LV_EVENT_CLICKED, NULL);

  char buf[64];
  auto addLine = [&](const char *text, uint32_t col) {
    lv_obj_t *lbl = lv_label_create(overlay);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(col), 0);
  };

  addLine("Device Info", 0xFFD700);

  lv_obj_t *sep = lv_obj_create(overlay);
  lv_obj_set_size(sep, LV_PCT(100), 1);
  lv_obj_set_style_bg_color(sep, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  String ssid = WiFi.SSID();
  snprintf(buf, sizeof(buf), "WiFi: %s", ssid.length() > 0 ? ssid.c_str() : "---");
  addLine(buf, 0xCCCCCC);

  snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
  addLine(buf, 0xCCCCCC);

  snprintf(buf, sizeof(buf), "Heap: %d KB free", (int)(ESP.getFreeHeap() / 1024));
  addLine(buf, 0xCCCCCC);

  uint32_t s = millis() / 1000;
  snprintf(buf, sizeof(buf), "Up: %dh %dm", (int)(s / 3600), (int)((s % 3600) / 60));
  addLine(buf, 0xCCCCCC);

  snprintf(buf, sizeof(buf), "Cities: %d  Bus stops: %d",
           (int)cityCount, (int)busStopCount);
  addLine(buf, 0xCCCCCC);

  addLine("Tap to close", 0x777777);
}
