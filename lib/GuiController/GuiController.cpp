#include "GuiController.h"
#include "BusService.h"
#include "DataManager.h"
#include "NetworkManager.h"
#include "SystemMonitor.h"
#include "WeatherService.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cstdio>
#include <lvgl.h>

// --- SCREEN DRIVER SETUP ---
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 320;
static lv_disp_draw_buf_t draw_buf;
static const uint32_t drawBufLines = 30;
static lv_color_t buf[screenWidth * drawBufLines];
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
std::atomic<GuiController::AppMode> GuiController::currentApp{
    GuiController::APP_WEATHER};

// Bus State
std::atomic<int> GuiController::currentBusIndex{0};
std::atomic<int> GuiController::busStopCount{1};
std::atomic<bool> GuiController::busStationChanged{false};
int GuiController::getBusIndex() { return currentBusIndex; }
void GuiController::setBusStopCount(int count) { busStopCount = count; }
bool GuiController::consumeBusStationChanged() {
  return busStationChanged.exchange(false);
}

// City State
std::atomic<int> GuiController::currentCityIndex{0};
std::atomic<int> GuiController::cityCount{1};
std::atomic<bool> GuiController::cityChanged{false};
int GuiController::getCityIndex() { return currentCityIndex; }
void GuiController::setCityCount(int count) { cityCount = count; }
bool GuiController::consumeCityChanged() { return cityChanged.exchange(false); }

bool GuiController::isBusScreenActive() { return currentApp == APP_BUS; }
bool GuiController::isStockScreenActive() { return currentApp == APP_STOCK; }
bool GuiController::isWeatherScreenActive() { return currentApp == APP_WEATHER; }

// Queue & Cache
String GuiController::pendingMsg = "";
volatile bool GuiController::needsUpdate = false;
GuiController::PendingScreen GuiController::pendingScreenChange = GuiController::SCREEN_NONE;
int GuiController::pendingScreenAnim = 0;
uint32_t GuiController::screenAnimUntil = 0;
int GuiController::pendingCitySwipeAnim = 0;
SemaphoreHandle_t GuiController::guiMutex = NULL;
WeatherData GuiController::cachedWeather;
BusData GuiController::cachedBus;
std::vector<StockItem> GuiController::cachedStock;

// Local Controller State
static lv_obj_t *activeTimeLabel = NULL;
static int forecastMode = 0; // 0: Current, 1: Hourly, 2: Daily, 3: Chart
static uint32_t lastGestureTime = 0; // Used to suppress tap after swipe

// --- In-place refresh state ---
// Objects on the visible screen that are updated without a rebuild. Each is
// cleared by an LV_EVENT_DELETE callback when LVGL deletes it (screen
// auto_del after a transition, or lv_obj_clean), so they never dangle.
static lv_obj_t *statusDot = NULL;
static lv_obj_t *scrollList = NULL;
static int scrollKey = -1;

static bool touchDown = false;
static uint32_t touchReleasedAt = 0;
static const uint32_t TOUCH_SETTLE_MS = 250; // After release, before rebuild

// A data source counts as stale (red dot) once it is this much older than
// its normal refresh interval, so the dot doesn't flap red right before
// every scheduled refresh.
static const uint32_t WEATHER_STALE_MS = 20UL * 60000UL; // Refresh: 15 min
static const uint32_t BUS_STALE_MS = 90000UL;            // Refresh: 60 s
static const uint32_t STOCK_STALE_MS = 7UL * 60000UL;    // Refresh: 5 min

static void onStatusDotDeleted(lv_event_t *e) {
  if (lv_event_get_target(e) == statusDot)
    statusDot = NULL;
}

static void onScrollListDeleted(lv_event_t *e) {
  if (lv_event_get_target(e) == scrollList)
    scrollList = NULL;
}

// --- Change detection: compare only what the views display ---

static bool sameDaily(const DailyForecast &a, const DailyForecast &b) {
  return a.date == b.date && a.maxTemp == b.maxTemp &&
         a.minTemp == b.minTemp && a.weatherCode == b.weatherCode &&
         a.pop == b.pop;
}

static bool sameHourly(const HourlyForecast &a, const HourlyForecast &b) {
  return a.time == b.time && a.temp == b.temp &&
         a.weatherCode == b.weatherCode && a.pop == b.pop &&
         a.isNight == b.isNight;
}

// lastUpdate itself is ignored (only the dot shows it). Placeholders
// (lastUpdate 0) never compare equal: their "Fetching..." / "Retrying"
// text depends on the fetch status, and they are cheap to rebuild.
static bool sameWeatherDisplay(const WeatherData &a, const WeatherData &b) {
  if (a.lastUpdate == 0 || b.lastUpdate == 0)
    return false;
  if (a.cityName != b.cityName || a.currentTemp != b.currentTemp ||
      a.currentWeatherCode != b.currentWeatherCode ||
      a.currentHumidity != b.currentHumidity ||
      a.currentPressure != b.currentPressure ||
      a.currentFeelsLike != b.currentFeelsLike ||
      a.currentAQI != b.currentAQI || a.windSpeed != b.windSpeed ||
      a.windDirection != b.windDirection ||
      a.currentRainProb != b.currentRainProb || a.isNight != b.isNight ||
      a.sunrise != b.sunrise || a.sunset != b.sunset)
    return false;
  for (int i = 0; i < 7; i++)
    if (!sameDaily(a.daily[i], b.daily[i]))
      return false;
  for (int i = 0; i < 24; i++)
    if (!sameHourly(a.hourly[i], b.hourly[i]))
      return false;
  return true;
}

// Same rows (line + destination, in order) => the ETAs can be updated in
// place. Only the rows BusView shows are compared.
static bool sameBusLayout(const BusData &a, const BusData &b) {
  if (a.stopName != b.stopName || a.stopCode != b.stopCode)
    return false;
  // Empty lists show a message; only "No buses right now" (real data, no
  // arrivals) is stable. Placeholder messages depend on the fetch status.
  if (a.arrivals.empty() || b.arrivals.empty())
    return a.arrivals.empty() && b.arrivals.empty() && a.lastUpdate != 0 &&
           b.lastUpdate != 0;
  size_t rowsA = min(a.arrivals.size(), (size_t)BusView::MAX_ROWS);
  size_t rowsB = min(b.arrivals.size(), (size_t)BusView::MAX_ROWS);
  if (rowsA != rowsB)
    return false;
  for (size_t i = 0; i < rowsA; i++)
    if (a.arrivals[i].line != b.arrivals[i].line ||
        a.arrivals[i].destination != b.arrivals[i].destination)
      return false;
  return true;
}

static bool sameStocks(const std::vector<StockItem> &a,
                       const std::vector<StockItem> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); i++)
    if (a[i].symbol != b[i].symbol || a[i].price != b[i].price ||
        a[i].changePercent != b[i].changePercent ||
        a[i].currency != b[i].currency)
      return false;
  return true;
}

LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);

void GuiController::init() {
  guiMutex = xSemaphoreCreateMutex();
  lv_init();
  Fonts::init();
  tft.begin();
  tft.setRotation(0);
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * drawBufLines);
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
  if ((int32_t)(millis() - screenAnimUntil) < 0) // wrap-safe
    return;
  // Data refreshes (anim 0) wait until the finger is off the screen, so a
  // rebuild never deletes the object being pressed. User navigation (swipe,
  // tap with an animation) applies immediately.
  if (pendingScreenAnim == 0 &&
      (touchDown || millis() - touchReleasedAt < TOUCH_SETTLE_MS))
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

// --- DATA APPLICATION (called from the main loop) ---

void GuiController::applyWeatherData(const WeatherData &data) {
  static bool shownOnce = false;
  bool changed = !sameWeatherDisplay(cachedWeather, data);
  cachedWeather = data;

  if (!shownOnce) { // First data replaces the boot loading screen
    shownOnce = true;
    requestRefresh();
    return;
  }
  if (currentApp != APP_WEATHER)
    return;
  if (changed)
    requestRefresh();
  else
    refreshStatusDot(); // Same numbers, new timestamp: dot only
}

void GuiController::applyBusData(const BusData &data) {
  bool sameLayout = sameBusLayout(cachedBus, data);
  cachedBus = data;

  if (currentApp != APP_BUS)
    return;
  // With a rebuild already queued the visible rows may belong to older data;
  // the rebuild will show this data anyway.
  if (sameLayout && pendingScreenChange == SCREEN_NONE) {
    BusView::updateEtas(data); // Same rows: just new arrival times
    refreshStatusDot();
  } else {
    requestRefresh();
  }
}

void GuiController::applyStockData(const std::vector<StockItem> &data) {
  bool changed = !sameStocks(cachedStock, data);
  cachedStock = data;

  if (currentApp != APP_STOCK)
    return;
  if (changed)
    requestRefresh();
  else
    refreshStatusDot();
}

void GuiController::onStatusChanged() {
  // Placeholder screens show "Fetching..." text that depends on the status,
  // so those are rebuilt; normal screens only recolour the dot.
  bool placeholder =
      (currentApp == APP_WEATHER && cachedWeather.lastUpdate == 0) ||
      (currentApp == APP_BUS && cachedBus.lastUpdate == 0 &&
       cachedBus.arrivals.empty());
  if (placeholder)
    requestRefresh();
  else
    refreshStatusDot();
}

// --- STATUS DOT ---

void GuiController::setStatusDot(lv_obj_t *dot) {
  statusDot = dot;
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE); // Touches go to the header
  lv_obj_add_event_cb(dot, onStatusDotDeleted, LV_EVENT_DELETE, NULL);
}

uint32_t GuiController::statusDotColor() {
  bool updating;
  uint32_t last;
  uint32_t staleMs;
  switch (currentApp.load()) {
  case APP_WEATHER:
    updating = DataManager::isWeatherUpdating(getCityIndex());
    last = cachedWeather.lastUpdate;
    staleMs = WEATHER_STALE_MS;
    break;
  case APP_BUS:
    updating = DataManager::isBusUpdating(getBusIndex());
    last = cachedBus.lastUpdate;
    staleMs = BUS_STALE_MS;
    break;
  default:
    updating = DataManager::isStockUpdating();
    last = DataManager::getStockLastUpdate();
    staleMs = STOCK_STALE_MS;
    break;
  }
  if (updating)
    return 0xFFFF00; // Yellow: fetching
  if (last == 0 || millis() - last > staleMs)
    return 0xFF0000; // Red: no data / stale
  return 0x00AA00;   // Green: fresh
}

void GuiController::refreshStatusDot() {
  if (statusDot)
    lv_obj_set_style_bg_color(statusDot, lv_color_hex(statusDotColor()), 0);
}

// --- TOUCH / SCROLL ---

void GuiController::setTouchActive(bool down) {
  if (touchDown && !down)
    touchReleasedAt = millis();
  touchDown = down;
}

void GuiController::trackListScroll(lv_obj_t *list, int key) {
  // The previous screen (and its list) still exists while the new one is
  // built, so its scroll offset can be carried over.
  lv_coord_t y = 0;
  if (scrollList && scrollKey == key)
    y = lv_obj_get_scroll_y(scrollList);

  scrollList = list;
  scrollKey = key;
  lv_obj_add_event_cb(list, onScrollListDeleted, LV_EVENT_DELETE, NULL);

  if (y > 0) {
    lv_obj_update_layout(list); // Children need sizes before scrolling
    lv_obj_scroll_to_y(list, y, LV_ANIM_OFF);
  }
}

void GuiController::requestRefresh() {
  // Only set pending if nothing else is already queued (don't overwrite a
  // swipe animation with a background data refresh).
  if (pendingScreenChange != SCREEN_NONE)
    return;
  switch (currentApp.load()) {
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
  BusView::forgetLiveLabels(); // Their labels are deleted by the clean below
  lv_obj_clean(scr);
  activeTimeLabel = NULL;

  // Same black theme as the app screens
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Weather Clock");
  lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_text(label, msg ? msg : "Loading...");
  lv_obj_set_width(label, screenWidth - 20);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xDDDDDD), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 10);
}

String GuiController::sanitize(const String &text) {
  // The UI fonts (Fonts::text14/text20) cover ASCII, Latin-1 (U+00A0..00FF)
  // and the euro sign. Keep those, map common typographic characters to
  // ASCII and replace anything else with '?' instead of drawing nothing.
  String out;
  out.reserve(text.length());
  const uint8_t *s = (const uint8_t *)text.c_str();
  size_t n = text.length();
  size_t i = 0;

  while (i < n) {
    uint8_t c = s[i];
    if (c < 0x80) {
      out += (char)c;
      i++;
      continue;
    }

    // Decode one UTF-8 sequence
    size_t len;
    uint32_t cp;
    if ((c & 0xE0) == 0xC0) {
      len = 2;
      cp = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
      len = 3;
      cp = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
      len = 4;
      cp = c & 0x07;
    } else {
      i++; // Stray continuation byte
      continue;
    }
    if (i + len > n)
      break; // Truncated sequence
    bool valid = true;
    for (size_t k = 1; k < len; k++) {
      if ((s[i + k] & 0xC0) != 0x80) {
        valid = false;
        break;
      }
      cp = (cp << 6) | (s[i + k] & 0x3F);
    }
    if (!valid) {
      i++;
      continue;
    }

    if ((cp >= 0xA0 && cp <= 0xFF) || cp == 0x20AC) {
      for (size_t k = 0; k < len; k++)
        out += (char)s[i + k]; // Covered by the fallback font
    } else if (cp == 0x2018 || cp == 0x2019) {
      out += '\'';
    } else if (cp == 0x201C || cp == 0x201D) {
      out += '"';
    } else if (cp == 0x2013 || cp == 0x2014) {
      out += '-';
    } else if (cp == 0x2026) {
      out += "...";
    } else {
      out += '?';
    }
    i += len;
  }
  return out;
}

void GuiController::createPageDots(lv_obj_t *parent, int count, int active) {
  if (count <= 1)
    return;

  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_SIZE_CONTENT, 6);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 5, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(row, LV_ALIGN_BOTTOM_LEFT, 2, 2);

  for (int i = 0; i < count; i++) {
    bool isActive = (i == active);
    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, isActive ? 14 : 6, 6); // Active page is a pill
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(
        dot, lv_color_hex(isActive ? 0x00FFFF : 0x555555), 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
  }
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
  BusView::tick(); // Live bus ETA countdown (no-op unless the bus screen is up)
  refreshStatusDot(); // Turns red once data goes stale, without a rebuild

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

  Serial.printf("GESTURE: dir=%d, app=%d\n", dir, (int)currentApp.load());
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
    forecastMode = (forecastMode + 1) % 3;
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

// --- LONG PRESS: manual refresh (body) and device info (header) ---

static void onBodyLongPress(lv_event_t *e) {
  switch (GuiController::currentApp.load()) {
  case GuiController::APP_WEATHER:
    DataManager::triggerWeatherUpdate();
    break;
  case GuiController::APP_BUS:
    DataManager::triggerBusUpdate();
    break;
  case GuiController::APP_STOCK:
    DataManager::triggerStockUpdate();
    break;
  }
  Serial.println("GUI: Long press -> manual refresh");
}

static void onInfoOverlayClicked(lv_event_t *e) {
  lv_obj_del_async(lv_event_get_target(e));
}

static void onHeaderLongPress(lv_event_t *e) {
  lv_event_stop_bubbling(e); // Not also a manual refresh

  SystemMonitor::Stats st = SystemMonitor::read();

  // On the active screen, so a screen rebuild also removes it
  lv_obj_t *overlay = lv_obj_create(lv_scr_act());
  lv_obj_set_size(overlay, 214, LV_SIZE_CONTENT);
  lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(overlay, lv_color_hex(0x111122), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
  lv_obj_set_style_radius(overlay, 12, 0);
  lv_obj_set_style_border_color(overlay, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_style_border_width(overlay, 1, 0);
  lv_obj_set_style_pad_all(overlay, 10, 0);
  lv_obj_set_style_pad_row(overlay, 4, 0);
  lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE); // Swallows the closing tap
  lv_obj_add_event_cb(overlay, onInfoOverlayClicked, LV_EVENT_CLICKED, NULL);

  auto addLine = [overlay](const char *text, uint32_t color) {
    lv_obj_t *lbl = lv_label_create(overlay);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &Fonts::text14, 0); // SSID may have accents
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
  };

  char buf[64];
  addLine("Device Info", 0xFFD700);
  String ssid = GuiController::sanitize(WiFi.SSID());
  snprintf(buf, sizeof(buf), "WiFi: %s (%d dBm)",
           ssid.length() ? ssid.c_str() : "---", st.rssi);
  addLine(buf, 0xCCCCCC);
  snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
  addLine(buf, 0xCCCCCC);
  addLine("    weatherclock.local", 0xCCCCCC);
  snprintf(buf, sizeof(buf), "Up: %s", SystemMonitor::formatUptime(st.uptimeS).c_str());
  addLine(buf, 0xCCCCCC);
  snprintf(buf, sizeof(buf), "Heap: %u KB (min %u KB)", st.heapFree / 1024,
           st.heapMinFree / 1024);
  addLine(buf, 0xCCCCCC);
  snprintf(buf, sizeof(buf), "Stack: loop %d, net %d", st.loopStackFree,
           st.netStackFree);
  addLine(buf, 0xCCCCCC);
  snprintf(buf, sizeof(buf), "Build: %s", __DATE__);
  addLine(buf, 0xCCCCCC);
  addLine("Tap to close", 0x777777);
}

void GuiController::attachLongPress(lv_obj_t *body, lv_obj_t *header) {
  lv_obj_add_event_cb(body, onBodyLongPress, LV_EVENT_LONG_PRESSED, NULL);
  // The header must be clickable to receive presses; taps and swipes on it
  // still bubble to the screen.
  lv_obj_add_flag(header, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE |
                              LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(header, onHeaderLongPress, LV_EVENT_LONG_PRESSED, NULL);
}
