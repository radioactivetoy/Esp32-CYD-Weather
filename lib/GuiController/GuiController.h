#pragma once

#include <Arduino.h>
#include <atomic>

#include "BusService.h"
#include "Fonts.h"
#include "StockService.h"
#include "WeatherService.h"
#include "lvgl.h"
#include "weather_icons.h"

// Include Views
#include "BusView.h"
#include "StockView.h"
#include "WeatherView.h"

class GuiController {
public:
  static void init();

  // These now delegate to Views
  static void showWeatherScreen(const WeatherData &data, int anim = -1);
  static void showBusScreen(const BusData &data, int anim = -1);
  static void showStockScreen(const std::vector<StockItem> &data,
                              int anim = -1);

  static void update(); // Main loop driver
  static void showLoadingScreen(const char *msg = nullptr);
  static void requestRefresh(); // Deferred refresh of the active screen
  static void updateTime();                        // Efficient clock update
  static void setActiveTimeLabel(lv_obj_t *label); // New setter
  static String sanitize(const String &text); // Fit text to the UI fonts
  // "● ○ ○" page indicator aligned to the parent's bottom-left (count > 1)
  static void createPageDots(lv_obj_t *parent, int count, int active);

  static bool isBusScreenActive();
  static bool isStockScreenActive();
  static bool isWeatherScreenActive();

  // Store fresh data and apply it to the visible screen with the least work:
  // a full rebuild only when something displayed changed; otherwise just
  // the status dot (and, for bus, the ETAs) are updated in place.
  static void applyWeatherData(const WeatherData &data);
  static void applyBusData(const BusData &data);
  static void applyStockData(const std::vector<StockItem> &data);
  // A fetch started/finished for the visible screen ("updating" dot)
  static void onStatusChanged();

  // Status dot of the visible screen, recoloured in place
  static void setStatusDot(lv_obj_t *dot);
  static uint32_t statusDotColor(); // For the current app's cached data
  static void refreshStatusDot();

  // Data refreshes wait while a finger is on the screen (set by the touch
  // driver callback), so a rebuild never swallows a tap or swipe.
  static void setTouchActive(bool down);

  // Keeps a scrollable list's position across rebuilds of the same view.
  // Call after the list's children exist. key identifies the view (e.g.
  // city + forecast mode); a different key starts at the top.
  static void trackListScroll(lv_obj_t *list, int key);

  enum AppMode { APP_WEATHER, APP_STOCK, APP_BUS };
  static AppMode currentApp;

  // Multi-Bus Support
  static std::atomic<int> currentBusIndex;
  static std::atomic<int> busStopCount;
  static std::atomic<bool> busStationChanged;
  static int getBusIndex();
  static void setBusStopCount(int count);
  static bool consumeBusStationChanged(); // Returns and clears the flag

  // Multi-City Support
  static std::atomic<int> currentCityIndex;
  static std::atomic<int> cityCount;
  static std::atomic<bool> cityChanged;
  static int getCityIndex();
  static void setCityCount(int count);
  static bool consumeCityChanged(); // Returns and clears the flag

  // Public Callbacks for Views
  static void handleGesture(lv_event_t *e);
  static void handleScreenClick(lv_event_t *e);
  static void handleSwipe(int16_t dx, int16_t dy);
  static uint32_t getLastGestureTime();

private:
  static String pendingMsg;
  static volatile bool needsUpdate;
  static void drawLoadingScreen(const char *msg);

  // Deferred screen transitions — set inside LVGL event callbacks,
  // applied after lv_timer_handler() returns to avoid re-entrancy crashes.
  enum PendingScreen { SCREEN_NONE, SCREEN_WEATHER, SCREEN_BUS, SCREEN_STOCK };
  static PendingScreen pendingScreenChange;
  static int pendingScreenAnim;
  static uint32_t screenAnimUntil; // millis() deadline: ongoing anim must finish first
  static int pendingCitySwipeAnim; // -1/1 captured on swipe, consumed by requestRefresh
  static void applyPendingScreenChange();

  // Cache data for gestures/redraws
  static WeatherData cachedWeather;
  static BusData cachedBus;
  static std::vector<StockItem> cachedStock;

  static SemaphoreHandle_t guiMutex; // Thread Safety for Loading Msg & State
};
