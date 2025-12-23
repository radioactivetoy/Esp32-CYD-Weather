#pragma once

#include "StockService.h"
#include "lvgl.h"
#include <Arduino.h>
#include <vector>


class StockView {
public:
  static void show(const std::vector<StockItem> &data, int anim);
};
