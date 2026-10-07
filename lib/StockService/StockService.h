#ifndef STOCK_SERVICE_H
#define STOCK_SERVICE_H

#include <Arduino.h>
#include <vector>

struct StockItem {
  String symbol;
  float price = 0;
  float changePercent = 0;
  bool isValid = false;
};

class StockService {
public:
  static std::vector<StockItem> getQuotes(const String &symbols);
};

#endif
