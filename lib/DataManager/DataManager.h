#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

#include "BusService.h"
#include "StockService.h"
#include "WeatherService.h"

// Cache Structs (Moved from main.cpp)
struct CityWeatherCache {
  String cityName;
  WeatherData data;
  uint32_t lastUpdate;
  bool hasData;
};

struct BusStopCache {
  String id;
  BusData data;
  uint32_t lastUpdate;
};

class DataManager {
public:
  static void begin(); // Starts background task

  // Thread-Safe Data Access (Returns true if new data available since last
  // check)
  static bool getWeatherData(WeatherData &out);
  static bool getBusData(BusData &out);
  static bool getStockData(std::vector<StockItem> &out);

  // Getters for specific fields without consuming the "Updated" flag
  // (Used for initial display or non-event driven reads)
  static WeatherData getCurrentWeatherData();
  static BusData getCurrentBusData();
  static std::vector<StockItem> getCurrentStockData();

  // Trigger updates manually (e.g. from UI)
  static void triggerBusUpdate();
  static void triggerWeatherUpdate();
  static void triggerStockUpdate();

  // Status
  static bool isWeatherUpdating();
  static bool isBusUpdating();
  static bool isStockUpdating();
  static uint32_t getStockLastUpdate();

private:
  static void networkTask(void *parameter); // The background loop

  static SemaphoreHandle_t dataMutex;

  // State
  static WeatherData weatherData;
  static BusData busData;
  static std::vector<StockItem> stockData;

  // Update Flags (Volatile for ISR/Thread safety)
  static volatile bool weatherDataUpdated;
  static volatile bool busDataUpdated;
  static volatile bool stockDataUpdated;
  static volatile bool isUpdatingWeather;
  static volatile bool isUpdatingBus;
  static volatile bool isUpdatingStock;
  static volatile uint32_t stockLastUpdateTime;

  static volatile bool manualBusTrigger;
  static volatile bool manualWeatherTrigger;
  static volatile bool manualStockTrigger;

  // Caches
  static std::vector<CityWeatherCache> cityCaches;
  static std::vector<BusStopCache> busCaches;
};

#endif
