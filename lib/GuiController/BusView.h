#pragma once

#include "BusService.h"
#include "lvgl.h"
#include "weather_icons.h"
#include <Arduino.h>


class BusView {
public:
  static const int MAX_ROWS = 6; // Arrivals shown

  static void show(const BusData &data, int anim);
  static void tick(); // Count the visible ETAs down between fetches
  // New fetch with the same rows: update the ETAs without a rebuild
  static void updateEtas(const BusData &data);
  static void forgetLiveLabels(); // Call before deleting the screen's children

private:
  static lv_color_t getBusLineColor(const String &line, lv_color_t &textColor);
};
