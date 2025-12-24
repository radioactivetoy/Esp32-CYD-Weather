#include "GuiController.h"
#include "BusService.h"
#include "NetworkManager.h"
#include "WeatherService.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cstdio>
#include <lvgl.h>

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
int GuiController::currentBusIndex = 0;
int GuiController::busStopCount = 1;
bool GuiController::busStationChanged = false;
int GuiController::getBusIndex() { return currentBusIndex; }
void GuiController::setBusStopCount(int count) { busStopCount = count; }
bool GuiController::hasBusStationChanged() { return busStationChanged; }
void GuiController::clearBusStationChanged() { busStationChanged = false; }

// City State
int GuiController::currentCityIndex = 0;
int GuiController::cityCount = 1;
bool GuiController::cityChanged = false;
int GuiController::getCityIndex() { return currentCityIndex; }
void GuiController::setCityCount(int count) { cityCount = count; }
bool GuiController::hasCityChanged() { return cityChanged; }
void GuiController::clearCityChanged() { cityChanged = false; }

bool GuiController::isBusScreenActive() { return currentApp == APP_BUS; }
void GuiController::updateBusCache(const BusData &data) { cachedBus = data; }
bool GuiController::isStockScreenActive() { return currentApp == APP_STOCK; }
void GuiController::updateStockCache(const std::vector<StockItem> &data) {
  cachedStock = data;
}

// Queue & Cache
String GuiController::pendingMsg = "";
bool GuiController::needsUpdate = false;
WeatherData GuiController::cachedWeather;
BusData GuiController::cachedBus;
std::vector<StockItem> GuiController::cachedStock;

// Local Controller State
static lv_obj_t *activeTimeLabel = NULL;
static int forecastMode = 0; // 0: Current, 1: Hourly, 2: Daily, 3: Chart

// Custom Font
LV_FONT_DECLARE(font_intl_16);

void GuiController::init() {
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
  if (msg)
    pendingMsg = String(msg);
  else
    pendingMsg = "Loading...";
  needsUpdate = true;
}

void GuiController::update() {
  if (needsUpdate) {
    needsUpdate = false;
    drawLoadingScreen(pendingMsg.c_str());
  }
  lv_timer_handler();
}

void GuiController::drawLoadingScreen(const char *msg) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  activeTimeLabel = NULL;

  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0000AA), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_text(label, msg ? msg : "Loading...");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
  // Note: Fonts are declared in Views now, but we can assume default or
  // montserrat if available globally. Ideally Views handle all drawing. But for
  // loading screen, default font is fine or reuse montserrat if declared here.
  // For safety, let's use default font or declare one locally if needed.
  // Actually, let's just use default LVGL font for Loading to avoid
  // dependency/declaration mess or rely on what's available.
}

void GuiController::handle(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    lv_timer_handler();
    delay(5);
  }
}

const char *GuiController::sanitize(const String &text) {
  // Replace multi-byte UTF-8 sequences with single ASCII chars
  // Catalan/Spanish common chars

  static String buf;
  buf = text; // Copy to static buffer (reserves memory once)

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

  return buf.c_str();
}

// --- DELEGATED VIEW METHODS ---

void GuiController::showWeatherScreen(const WeatherData &data, int anim) {
  if (&data != &cachedWeather)
    cachedWeather = data;
  WeatherView::show(data, anim, forecastMode);
}

void GuiController::showBusScreen(const BusData &data, int anim) {
  if (&data != &cachedBus)
    cachedBus = data;
  BusView::show(data, anim);
}

void GuiController::showStockScreen(const std::vector<StockItem> &data,
                                    int anim) {
  if (&data != &cachedStock)
    cachedStock = data;
  StockView::show(data, anim);
}

// --- CONTROLLER LOGIC ---

void GuiController::setActiveTimeLabel(lv_obj_t *label) {
  activeTimeLabel = label;
}

void GuiController::updateTime() {
  if (activeTimeLabel == NULL)
    return;
  if (!lv_obj_is_valid(activeTimeLabel)) {
    activeTimeLabel = NULL;
    return;
  }

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

void GuiController::handleGesture(lv_event_t *e) {
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
  // Serial.printf("DEBUG: Gesture Dir: %d, App: %d\n", dir, currentApp);

  // --- CIRCULAR NAVIGATION (Up/Down) ---
  if (dir == LV_DIR_TOP) {
    // UP: Weather -> Stocks -> Bus -> Weather
    if (currentApp == APP_WEATHER) {
      showStockScreen(cachedStock, LV_SCR_LOAD_ANIM_MOVE_TOP);
    } else if (currentApp == APP_STOCK) {
      showBusScreen(cachedBus, LV_SCR_LOAD_ANIM_MOVE_TOP);
    } else if (currentApp == APP_BUS) {
      showWeatherScreen(cachedWeather, LV_SCR_LOAD_ANIM_MOVE_TOP);
    }
  } else if (dir == LV_DIR_BOTTOM) {
    // DOWN: Weather -> Bus -> Stocks -> Weather
    if (currentApp == APP_WEATHER) {
      showBusScreen(cachedBus, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
    } else if (currentApp == APP_BUS) {
      showStockScreen(cachedStock, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
    } else if (currentApp == APP_STOCK) {
      showWeatherScreen(cachedWeather, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
    }
  }
  // --- INTERNAL VIEWS (Left/Right) ---
  else if (dir == LV_DIR_LEFT) {
    if (currentApp == APP_WEATHER) {
      if (cityCount > 1) {
        currentCityIndex = (currentCityIndex + 1) % cityCount;
        cityChanged = true;
      }
    }
    // Bus Swipe Removed
  } else if (dir == LV_DIR_RIGHT) {
    if (currentApp == APP_WEATHER) {
      if (cityCount > 1) {
        currentCityIndex = (currentCityIndex - 1 + cityCount) % cityCount;
        cityChanged = true;
      }
    }
    // Bus Swipe Removed
  }
}

void GuiController::handleScreenClick(lv_event_t *e) {
  // Fix: Ignore click if a gesture was detected
  if (lv_indev_get_gesture_dir(lv_indev_get_act()) != LV_DIR_NONE) {
    return;
  }

  if (currentApp == APP_WEATHER) {
    forecastMode = (forecastMode + 1) % 4;
    showWeatherScreen(cachedWeather, LV_SCR_LOAD_ANIM_FADE_ON);
  } else if (currentApp == APP_BUS) {
    // Switch Station on Tap
    if (busStopCount > 1) {
      currentBusIndex = (currentBusIndex + 1) % busStopCount;
      busStationChanged = true;
    }
  }
}
