#pragma once

#include "BusService.h"
#include "lvgl.h"
#include "weather_icons.h"
#include <Arduino.h>


class BusView {
public:
  static void show(const BusData &data, int anim);
  static void tick(); // Count the visible ETAs down between fetches
  static void forgetLiveLabels(); // Call before deleting the screen's children

private:
  static lv_color_t getBusLineColor(const String &line, lv_color_t &textColor);
};
