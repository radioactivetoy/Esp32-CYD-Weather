#pragma once

#include "WeatherService.h"
#include "lvgl.h"
#include "weather_icons.h"
#include <Arduino.h>


class WeatherView {
public:
  static void show(const WeatherData &data, int anim, int forecastMode);

private:
  static void createWeatherIcon(lv_obj_t *parent, int code);
  static const char *getWeatherDesc(int code);
  static void formatDate(const char *input, char *output);
};
