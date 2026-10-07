#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

#include "BusService.h"
#include "StockService.h"
#include "WeatherService.h"

// Per-city state, owned by the network task.
struct CityWeatherCache {
  String cityName;
  WeatherData data;
  uint32_t lastUpdate = 0; // millis() of last success
  bool hasData = false;

  // Geocoding result, cached so we only look a city up once per boot
  bool hasCoords = false;
  float lat = 0;
  float lon = 0;
  String resolvedName;

  // Failure backoff
  uint32_t nextAttempt = 0;
  uint8_t failCount = 0;
};

// Per-stop state, owned by the network task.
struct BusStopCache {
  String id;
  BusData data;
  uint32_t lastUpdate = 0;
  bool hasData = false;
  uint32_t nextAttempt = 0;
  uint8_t failCount = 0;
};

class DataManager {
public:
  static void begin(); // Starts background task

  // Thread-Safe Data Access (Returns true if new data available since last
  // check)
  static bool getWeatherData(WeatherData &out);
  static bool getBusData(BusData &out);
  static bool getStockData(std::vector<StockItem> &out);

  // Trigger updates manually (e.g. from UI)
  static void triggerBusUpdate();
  static void triggerWeatherUpdate(); // Visible city, ignores backoff
  static void triggerStockUpdate();

  // Status
  static bool isWeatherUpdating(int cityIndex);
  static bool isBusUpdating(int busIndex);
  static bool isStockUpdating();
  static uint32_t getStockLastUpdate();

  // Status Change Signals (True when "Is Updating" state changes)
  static bool getWeatherStatusChanged();
  static bool getBusStatusChanged();

private:
  static void networkTask(void *parameter); // The background loop

  static SemaphoreHandle_t dataMutex;

  // State
  static WeatherData weatherData;
  static BusData busData;
  static std::vector<StockItem> stockData;

  // Update Flags (atomic for correct dual-core memory ordering on ESP32)
  static std::atomic<bool> weatherDataUpdated;
  static std::atomic<bool> busDataUpdated;
  static std::atomic<bool> stockDataUpdated;
  static std::atomic<int> currentUpdatingCityIndex;
  static std::atomic<int> currentUpdatingBusIndex;
  static std::atomic<bool> isUpdatingStock;
  static std::atomic<uint32_t> stockLastUpdateTime;

  static std::atomic<bool> weatherStatusChanged;
  static std::atomic<bool> busStatusChanged;

  static std::atomic<bool> manualBusTrigger;
  static std::atomic<bool> manualWeatherTrigger;
  static std::atomic<bool> manualStockTrigger;

  // Caches
  static std::vector<CityWeatherCache> cityCaches;
  static std::vector<BusStopCache> busCaches;
};

#endif
