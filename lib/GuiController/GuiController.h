#pragma once

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
  static void updateTime();                        // Efficient clock update
  static void setActiveTimeLabel(lv_obj_t *label); // New setter
  static const char *
  sanitize(const String &text); // Remove heavy accents (Returns static buffer)

  static bool isBusScreenActive();
  static bool isStockScreenActive();
  static void updateBusCache(const BusData &data);
  static void updateStockCache(const std::vector<StockItem> &data);

  enum AppMode { APP_WEATHER, APP_STOCK, APP_BUS };
  static AppMode currentApp;

  // Multi-Bus Support
  static int currentBusIndex;
  static int busStopCount;
  static bool busStationChanged;
  static int getBusIndex();
  static void setBusStopCount(int count);
  static bool hasBusStationChanged();
  static void clearBusStationChanged();

  // Multi-City Support
  static int currentCityIndex;
  static int cityCount;
  static bool cityChanged;
  static int getCityIndex();
  static void setCityCount(int count);
  static bool hasCityChanged();
  static void clearCityChanged();

  // Public Callbacks for Views
  static void handleGesture(lv_event_t *e);
  static void handleScreenClick(lv_event_t *e);

private:
  static String pendingMsg;
  static bool needsUpdate;
  static void drawLoadingScreen(const char *msg);

  // Cache data for gestures/redraws
  static WeatherData cachedWeather;
  static BusData cachedBus;
  static std::vector<StockItem> cachedStock;
};
