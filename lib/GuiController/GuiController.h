#pragma once

#include "BusService.h"
#include "StockService.h"
#include "WeatherService.h"
#include "lvgl.h"
#include "weather_icons.h"

class GuiController {
public:
  static void init();
  static void handle(uint32_t ms);
  static void showWeatherScreen(const WeatherData &data, int anim = -1);
  static void showBusScreen(const BusData &data, int anim = -1);
  static void update(); // Main loop driver
  static void showLoadingScreen(const char *msg = nullptr);
  static void updateTime(); // Efficient clock update
  static bool isBusScreenActive();
  static bool isStockScreenActive(); // New
  static void
  updateBusCache(const BusData &data); // New: Cache update without render
  static void updateStockCache(const std::vector<StockItem> &data); // New

private:
  static bool busScreenActive;
  static bool stockScreenActive;
  static String pendingMsg;
  static bool needsUpdate;
  static void drawLoadingScreen(const char *msg);

  // Cache data for gestures/redraws
  static WeatherData cachedWeather;
  static BusData cachedBus;
  static std::vector<StockItem> cachedStock;

public:
  static void showStockScreen(const std::vector<StockItem> &data,
                              int anim = -1);

private:
  static lv_color_t getBusLineColor(const String &line, lv_color_t &textColor);
  static void createWeatherIcon(lv_obj_t *parent, int code);

  static const char *getWeatherDesc(int code);
  static void handleGesture(lv_event_t *e);
};
