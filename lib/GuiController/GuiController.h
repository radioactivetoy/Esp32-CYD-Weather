#pragma once

#include <Arduino.h>
#include <atomic>

#include "BusService.h"
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
  static void handle(uint32_t ms);

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
  static String sanitize(const String &text);      // Safe Return by Value

  static bool isBusScreenActive();
  static bool isStockScreenActive();
  static bool isWeatherScreenActive();
  static void updateWeatherCache(const WeatherData &data);
  static void updateBusCache(const BusData &data);
  static void updateStockCache(const std::vector<StockItem> &data);

  enum AppMode { APP_WEATHER, APP_STOCK, APP_BUS };
  static AppMode currentApp;

  // Multi-Bus Support
  static std::atomic<int> currentBusIndex;
  static std::atomic<int> busStopCount;
  static std::atomic<bool> busStationChanged;
  static int getBusIndex();
  static void setBusStopCount(int count);
  static bool hasBusStationChanged();
  static void clearBusStationChanged();

  // Multi-City Support
  static std::atomic<int> currentCityIndex;
  static std::atomic<int> cityCount;
  static std::atomic<bool> cityChanged;
  static int getCityIndex();
  static void setCityCount(int count);
  static bool hasCityChanged();
  static void clearCityChanged();

  // Public Callbacks for Views
  static void handleGesture(lv_event_t *e);
  static void handleScreenClick(lv_event_t *e);
  static void handleLongPress(lv_event_t *e);
  static void showInfoOverlay(lv_event_t *e);
  static void handleSwipe(int16_t dx, int16_t dy);
  static uint32_t getLastGestureTime();

  static std::atomic<bool> longPressTriggered; // polled by main.cpp

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
