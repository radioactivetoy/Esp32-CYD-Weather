#ifndef STOCK_SERVICE_H
#define STOCK_SERVICE_H

#include <Arduino.h>
#include <vector>

struct StockItem {
  String symbol;
  float price;
  float changePercent;
  bool isValid;
};

class StockService {
public:
  static std::vector<StockItem> getQuotes(String symbols);
};

#endif
