#pragma once

#include "WeatherService.h"
#include <Arduino.h>

class LedController {
public:
  static void begin();
  static void update(const WeatherData &list);
  static void setRGB(uint8_t r, uint8_t g, uint8_t b); // 0-255

private:
  static bool isRain(int code);
};
