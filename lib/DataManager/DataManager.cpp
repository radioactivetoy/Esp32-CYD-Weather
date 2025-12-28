#include "DataManager.h"
#include "GuiController.h"
#include "LedController.h"
#include "NetworkManager.h"
#include <esp_task_wdt.h> // Hardware Watchdog

// Defines
SemaphoreHandle_t DataManager::dataMutex = NULL;

WeatherData DataManager::weatherData;
BusData DataManager::busData;
std::vector<StockItem> DataManager::stockData;

volatile bool DataManager::weatherDataUpdated = false;
volatile bool DataManager::busDataUpdated = false;
volatile bool DataManager::stockDataUpdated = false;

volatile bool DataManager::manualBusTrigger = false;
volatile bool DataManager::manualWeatherTrigger = false;
volatile bool DataManager::manualStockTrigger = true;
volatile bool DataManager::isUpdatingWeather = false;
volatile bool DataManager::isUpdatingBus = false;
volatile bool DataManager::isUpdatingStock = false;
volatile uint32_t DataManager::stockLastUpdateTime = 0;

bool DataManager::isWeatherUpdating() { return isUpdatingWeather; }
bool DataManager::isBusUpdating() { return isUpdatingBus; }
bool DataManager::isStockUpdating() { return isUpdatingStock; }
uint32_t DataManager::getStockLastUpdate() { return stockLastUpdateTime; }

std::vector<CityWeatherCache> DataManager::cityCaches;
std::vector<BusStopCache> DataManager::busCaches;

void DataManager::begin() {
  dataMutex = xSemaphoreCreateMutex();

  // Start Background Task
  // Stack size 10240 (same as before)
  xTaskCreatePinnedToCore(networkTask, "NetTask", 10240, NULL, 1, NULL, 0);
}

bool DataManager::getWeatherData(WeatherData &out) {
  bool updated = false;
  if (xSemaphoreTake(dataMutex, 5) == pdTRUE) { // Short wait
    if (weatherDataUpdated) {
      out = weatherData;
      weatherDataUpdated = false; // Clear flag
      updated = true;
    }
    xSemaphoreGive(dataMutex);
  }
  return updated;
}

bool DataManager::getBusData(BusData &out) {
  bool updated = false;
  if (xSemaphoreTake(dataMutex, 5) == pdTRUE) {
    if (busDataUpdated) {
      out = busData;
      busDataUpdated = false;
      updated = true;
    }
    xSemaphoreGive(dataMutex);
  }
  return updated;
}

bool DataManager::getStockData(std::vector<StockItem> &out) {
  bool updated = false;
  if (xSemaphoreTake(dataMutex, 5) == pdTRUE) {
    if (stockDataUpdated) {
      out = stockData;
      stockDataUpdated = false;
      updated = true;
    }
    xSemaphoreGive(dataMutex);
  }
  return updated;
}

// Getters without clearing flag
WeatherData DataManager::getCurrentWeatherData() {
  WeatherData temp;
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    temp = weatherData;
    xSemaphoreGive(dataMutex);
  }
  return temp;
}

BusData DataManager::getCurrentBusData() {
  BusData temp;
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    temp = busData;
    xSemaphoreGive(dataMutex);
  }
  return temp;
}

std::vector<StockItem> DataManager::getCurrentStockData() {
  std::vector<StockItem> temp;
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    temp = stockData;
    xSemaphoreGive(dataMutex);
  }
  return temp;
}

void DataManager::triggerBusUpdate() { manualBusTrigger = true; }
void DataManager::triggerWeatherUpdate() { manualWeatherTrigger = true; }
void DataManager::triggerStockUpdate() { manualStockTrigger = true; }

// --- BACKGROUND TASK (The "Brain") ---
void DataManager::networkTask(void *parameter) {
  // Wait for mutex
  while (dataMutex == NULL)
    vTaskDelay(10);

  // 1. Initial Connection / Loading
  // We can't call GuiController directly safely from here without mutex if Gui
  // uses it? Actually GuiController has its own internal state, but we should
  // be careful. Ideally we post messages. But for now, we follow the pattern:
  // "GuiController::showLoadingScreen" is static and writes to a queue var.

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    GuiController::showLoadingScreen("Connecting WiFi...");
    xSemaphoreGive(dataMutex);
  }

  NetworkManager::begin();

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    GuiController::showLoadingScreen("Fetching Weather...");
    xSemaphoreGive(dataMutex);
  }

  // --- INITIAL DATA SETUP ---

  // 1. Cities
  std::vector<String> cities = NetworkManager::getCities();
  GuiController::setCityCount(cities.size()); // Notify GUI of count

  cityCaches.resize(cities.size());
  for (size_t i = 0; i < cities.size(); i++) {
    cityCaches[i].cityName = cities[i];
    cityCaches[i].lastUpdate = 0;
    cityCaches[i].hasData = false;
  }

  // 2. Initial Weather Fetch (City 0)
  if (!cities.empty()) {
    Serial.printf("NETWORK: Fetching Primary City: %s\n", cities[0].c_str());
    WeatherData tempWeather;
    float pLat, pLon;
    String pResolved;
    bool primarySuccess = false;

    if (WeatherService::lookupCoordinates(cities[0], pLat, pLon, pResolved,
                                          NetworkManager::getOwmApiKey())) {
      String owmKey = NetworkManager::getOwmApiKey();
      if (WeatherService::updateWeather(tempWeather, pLat, pLon, owmKey)) {
        tempWeather.cityName = (pResolved.length() > 0) ? pResolved : cities[0];
        tempWeather.lastUpdate = millis(); // Set Timestamp

        cityCaches[0].data = tempWeather;
        cityCaches[0].hasData = true;
        cityCaches[0].lastUpdate = tempWeather.lastUpdate;

        // Push to Global
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
          weatherData = tempWeather;
          weatherDataUpdated = true;          // Flag for main loop
          LedController::update(weatherData); // LED logic
          xSemaphoreGive(dataMutex);
        }
        primarySuccess = true;
      }
    }
    if (!primarySuccess)
      Serial.println("NETWORK: Failed initial primary fetch.");
  } else {
    Serial.println("NETWORK: No cities configured.");
  }

  // 3. Bus Setup
  std::vector<String> stopIds = NetworkManager::getBusStops();
  GuiController::setBusStopCount(stopIds.size());

  busCaches.resize(stopIds.size());
  for (size_t i = 0; i < stopIds.size(); i++) {
    busCaches[i].id = stopIds[i];
    busCaches[i].lastUpdate = 0;
  }

  // 4. Initial Bus Fetch (Stop 0)
  if (stopIds.size() > 0) {
    String stopId = stopIds[0];
    Serial.printf("NETWORK: Fetching Primary Bus Stop: %s\n", stopId.c_str());
    BusData tempBus;
    if (BusService::updateBusTimes(tempBus, stopId,
                                   NetworkManager::getAppId().c_str(),
                                   NetworkManager::getAppKey().c_str())) {
      tempBus.lastUpdate = millis();
      busCaches[0].data = tempBus;
      busCaches[0].lastUpdate = tempBus.lastUpdate;

      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        busData = tempBus;
        busDataUpdated = true;
        xSemaphoreGive(dataMutex);
      }
    } else {
      Serial.println("NETWORK: Failed initial bus fetch.");
    }
  }

  // Tell GUI we are ready to show main screen?
  // In old code, main.cpp set `pendingWeatherRedraw = true` after setup.
  // We can simulate this by setting `weatherDataUpdated = true` (already done
  // above). The main loop will notice this and draw.

  uint32_t lastStockUpdate = 0;
  uint32_t lastNetworkRequestMs = 0; // Rate Limiter

  // --- MAIN LOOP ---
  for (;;) {
    uint32_t now = millis();

    // Rate Limiting Check (Min 1s between requests)
    bool safeToRequest = (now - lastNetworkRequestMs > 1000);

    // ---------------- WEATHER ----------------
    // ---------------- WEATHER ----------------
    int targetCityIndex = GuiController::getCityIndex();
    bool citySwitched = GuiController::hasCityChanged();
    if (citySwitched)
      GuiController::clearCityChanged();

    // Prioritize Manual Trigger or Switched City (Immediate Update if Stale)
    int cityToUpdate = -1;

    // Only process manual trigger if safe to request
    if ((manualWeatherTrigger || (citySwitched && targetCityIndex >= 0)) &&
        safeToRequest) {
      // If switched, only update if stale (> 10 mins) or no data
      if (manualWeatherTrigger || !cityCaches[targetCityIndex].hasData ||
          (now - cityCaches[targetCityIndex].lastUpdate > 600000)) {
        cityToUpdate = targetCityIndex;
      }
      manualWeatherTrigger = false;
    }

    // If no priority update, check for Background Updates (All Cities)
    if (cityToUpdate == -1) {
      for (size_t i = 0; i < cityCaches.size(); i++) {
        // Update if never updated (startup) OR stale > 15 mins (900s)
        if (cityCaches[i].lastUpdate == 0 ||
            (now - cityCaches[i].lastUpdate > 900000)) {
          cityToUpdate = i;
          break; // Update one per loop to yield to Bus/Stocks
        }
      }
    }

    // Execute Update
    if (cityToUpdate >= 0 && cityToUpdate < (int)cityCaches.size()) {
      if (safeToRequest) {
        Serial.printf("NETWORK: Updating City %d: %s\n", cityToUpdate,
                      cityCaches[cityToUpdate].cityName.c_str());
        lastNetworkRequestMs = now; // Update timestamp

        WeatherData temp;
        float lat, lon;
        String res;
        String owmKey = NetworkManager::getOwmApiKey();

        if (WeatherService::lookupCoordinates(cityCaches[cityToUpdate].cityName,
                                              lat, lon, res, owmKey)) {
          isUpdatingWeather = true;
          bool success = WeatherService::updateWeather(temp, lat, lon, owmKey);
          isUpdatingWeather = false;

          if (success) {
            temp.cityName =
                (res.length() > 0) ? res : cityCaches[cityToUpdate].cityName;
            temp.lastUpdate = now; // Set Timestamp

            cityCaches[cityToUpdate].data = temp;
            cityCaches[cityToUpdate].lastUpdate = now;
            cityCaches[cityToUpdate].hasData = true;

            // If we updated the currently active city, push to global
            // immediately
            if (cityToUpdate == targetCityIndex) {
              xSemaphoreTake(dataMutex, portMAX_DELAY);
              weatherData = temp;
              weatherDataUpdated = true;
              xSemaphoreGive(dataMutex);
            }

            if (cityToUpdate == 0) {
              LedController::update(temp);
            }
          }
          // Always trigger update to clear "Updating" status in UI
          if (cityToUpdate == targetCityIndex) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            if (!success) {
              // If failed, we still want to notify UI to redraw (to clear
              // Yellow dot) We don't update weatherData, just the flag.
            }
            weatherDataUpdated = true;
            xSemaphoreGive(dataMutex);
          }
        }
      } // End if (safeToRequest)
    } // End if (cityToUpdate >= 0)

    // If we just switched to a cached city (and didn't need update), load
    // from cache
    if (citySwitched && cityToUpdate != targetCityIndex &&
        targetCityIndex >= 0 && cityCaches[targetCityIndex].hasData) {
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      weatherData = cityCaches[targetCityIndex].data;
      weatherDataUpdated = true;
      // Let's ensure LED is updated on switch too, BUT ONLY IF switching
      // to Primary City
      if (targetCityIndex == 0) {
        LedController::update(weatherData);
      }
      xSemaphoreGive(dataMutex);
    }

    // ---------------- BUS ----------------
    int targetBusIndex = GuiController::getBusIndex();
    bool stationChanged = GuiController::hasBusStationChanged();
    if (stationChanged)
      GuiController::clearBusStationChanged();

    int busToUpdate = -1;

    // Prioritize Manual Trigger or Change
    if ((manualBusTrigger || (stationChanged && targetBusIndex >= 0)) &&
        safeToRequest) {
      if (manualBusTrigger ||
          busCaches[targetBusIndex].data.stopCode.isEmpty() ||
          (now - busCaches[targetBusIndex].lastUpdate > 60000)) {
        busToUpdate = targetBusIndex;
      } else {
        // Cache Hit
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        busData = busCaches[targetBusIndex].data;
        busDataUpdated = true;
        xSemaphoreGive(dataMutex);
      }
      manualBusTrigger = false;
    }

    // Background Updates (All Stops)
    if (busToUpdate == -1) {
      for (size_t i = 0; i < busCaches.size(); i++) {
        // Update if never updated (startup) OR stale > 60s
        if (busCaches[i].lastUpdate == 0 ||
            (now - busCaches[i].lastUpdate > 60000)) {
          busToUpdate = i;
          break;
        }
      }
    }

    // Execute Update
    if (busToUpdate >= 0 && busToUpdate < (int)busCaches.size()) {
      if (safeToRequest) {
        String stopId = busCaches[busToUpdate].id;
        Serial.printf("NETWORK: Updating Bus Stop %s...\n", stopId.c_str());
        lastNetworkRequestMs = now;

        BusData tempBus;
        isUpdatingBus = true;
        bool success = BusService::updateBusTimes(
            tempBus, stopId, NetworkManager::getAppId().c_str(),
            NetworkManager::getAppKey().c_str());
        isUpdatingBus = false;

        if (success) {
          tempBus.lastUpdate = now;
          busCaches[busToUpdate].data = tempBus;
          busCaches[busToUpdate].lastUpdate = now;
        }
      }

      // Always trigger update to clear "Updating" status
      if (busToUpdate == targetBusIndex) {
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        busDataUpdated = true;
        xSemaphoreGive(dataMutex);
      }
    }

    // ---------------- STOCK ----------------
    if ((now - lastStockUpdate > 300000 || manualStockTrigger) &&
        safeToRequest) {
      manualStockTrigger = false;
      String syms = NetworkManager::getStockSymbols();
      if (syms.length() > 0) {
        Serial.println("NETWORK: Updating Stocks...");
        lastNetworkRequestMs = now;

        isUpdatingStock = true;
        std::vector<StockItem> items = StockService::getQuotes(syms);
        isUpdatingStock = false;

        if (!items.empty()) {
          xSemaphoreTake(dataMutex, portMAX_DELAY);
          stockData = items;
          stockLastUpdateTime = now;
          stockDataUpdated = true;
          xSemaphoreGive(dataMutex);
        } else {
          // Failed or empty - still update flag to clear status
          xSemaphoreTake(dataMutex, portMAX_DELAY);
          stockDataUpdated = true;
          xSemaphoreGive(dataMutex);
        }
      }
      lastStockUpdate = now;
    }

    NetworkManager::handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
