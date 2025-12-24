#include "DataManager.h"
#include "GuiController.h"
#include "LedController.h"
#include "NetworkManager.h"

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
volatile bool DataManager::manualStockTrigger = true; // Force initial

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
  Serial.printf("NETWORK: Fetching Primary City: %s\n", cities[0].c_str());
  WeatherData tempWeather;
  float pLat, pLon;
  String pResolved;
  bool primarySuccess = false;

  if (WeatherService::lookupCoordinates(cities[0], pLat, pLon, pResolved)) {
    String owmKey = NetworkManager::getOwmApiKey();
    if (WeatherService::updateWeather(tempWeather, pLat, pLon, owmKey)) {
      tempWeather.cityName = (pResolved.length() > 0) ? pResolved : cities[0];

      cityCaches[0].data = tempWeather;
      cityCaches[0].hasData = true;
      cityCaches[0].lastUpdate = millis();

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

  // 3. Bus Setup
  std::vector<String> stopIds = NetworkManager::getBusStops();
  GuiController::setBusStopCount(stopIds.size());

  busCaches.resize(stopIds.size());
  for (size_t i = 0; i < stopIds.size(); i++) {
    busCaches[i].id = stopIds[i];
    busCaches[i].lastUpdate = 0;
  }

  // Tell GUI we are ready to show main screen?
  // In old code, main.cpp set `pendingWeatherRedraw = true` after setup.
  // We can simulate this by setting `weatherDataUpdated = true` (already done
  // above). The main loop will notice this and draw.

  uint32_t lastStockUpdate = 0;

  // --- MAIN LOOP ---
  for (;;) {
    uint32_t now = millis();

    // ---------------- WEATHER ----------------
    int targetCityIndex = GuiController::getCityIndex();
    bool citySwitched =
        GuiController::hasCityChanged(); // Note: GuiController must handle
                                         // clearing this?
    // Actually DataManager reading it doesn't clear it from GuiController side
    // if we access directly? Reading `GuiController::hasCityChanged()` returns
    // the bool. We need to clear it. `GuiController::clearCityChanged()`.

    // A. Primary City Background Update (Every 10m)
    if (cityCaches.size() > 0 && (now - cityCaches[0].lastUpdate > 600000)) {
      Serial.println("NETWORK: Updating Primary City (Background)...");
      WeatherData temp;
      float lat, lon;
      String res;
      if (WeatherService::lookupCoordinates(cityCaches[0].cityName, lat, lon,
                                            res)) {
        String owmKey = NetworkManager::getOwmApiKey();
        if (WeatherService::updateWeather(temp, lat, lon, owmKey)) {
          temp.cityName = (res.length() > 0) ? res : cityCaches[0].cityName;
          cityCaches[0].data = temp;
          cityCaches[0].lastUpdate = now;
          cityCaches[0].hasData = true;

          // Update Global
          xSemaphoreTake(dataMutex, portMAX_DELAY);
          if (targetCityIndex == 0) {
            weatherData = temp;
            weatherDataUpdated = true;
          }
          LedController::update(temp);
          xSemaphoreGive(dataMutex);
        }
      }
    }

    // B. Active City Logic
    if (targetCityIndex >= 0 && targetCityIndex < (int)cityCaches.size()) {
      bool needFetch = false;
      if (citySwitched) {
        if (!cityCaches[targetCityIndex].hasData ||
            (now - cityCaches[targetCityIndex].lastUpdate > 600000)) {
          needFetch = true;
        } else {
          // Cache Hit
          xSemaphoreTake(dataMutex, portMAX_DELAY);
          weatherData = cityCaches[targetCityIndex].data;
          weatherDataUpdated = true;
          xSemaphoreGive(dataMutex);
          GuiController::clearCityChanged();
        }
      } else if (targetCityIndex != 0) {
        // Background update for secondary active city
        if (now - cityCaches[targetCityIndex].lastUpdate > 600000)
          needFetch = true;
      }

      if (needFetch || manualWeatherTrigger) {
        manualWeatherTrigger = false;
        Serial.printf("NETWORK: Fetching City %d: %s\n", targetCityIndex,
                      cityCaches[targetCityIndex].cityName.c_str());
        WeatherData temp;
        float lat, lon;
        String res;
        if (WeatherService::lookupCoordinates(
                cityCaches[targetCityIndex].cityName, lat, lon, res)) {
          String owmKey = NetworkManager::getOwmApiKey();
          if (WeatherService::updateWeather(temp, lat, lon, owmKey)) {
            temp.cityName =
                (res.length() > 0) ? res : cityCaches[targetCityIndex].cityName;
            cityCaches[targetCityIndex].data = temp;
            cityCaches[targetCityIndex].lastUpdate = now;
            cityCaches[targetCityIndex].hasData = true;

            xSemaphoreTake(dataMutex, portMAX_DELAY);
            weatherData = temp;
            weatherDataUpdated = true;
            xSemaphoreGive(dataMutex);
            GuiController::clearCityChanged();
          }
        }
      }
    }

    // ---------------- BUS ----------------
    int targetBusIndex = GuiController::getBusIndex();
    bool stationChanged = GuiController::hasBusStationChanged();

    // Detect App Switch to Bus (Handled in main.cpp originally via "became
    // active") We can expose `triggerBusUpdate` for that.

    if (targetBusIndex >= 0 && targetBusIndex < (int)busCaches.size()) {
      bool needFetch = false;

      if (stationChanged) {
        if (now - busCaches[targetBusIndex].lastUpdate > 60000 ||
            busCaches[targetBusIndex].data.stopCode.isEmpty()) {
          needFetch = true;
        } else {
          // Cache Hit
          xSemaphoreTake(dataMutex, portMAX_DELAY);
          busData = busCaches[targetBusIndex].data;
          busDataUpdated = true;
          xSemaphoreGive(dataMutex);
          GuiController::clearBusStationChanged();
        }
      } else if (GuiController::isBusScreenActive()) {
        if (now - busCaches[targetBusIndex].lastUpdate > 60000)
          needFetch = true;
      }

      if (needFetch || manualBusTrigger) {
        manualBusTrigger = false;
        String stopId = busCaches[targetBusIndex].id;
        Serial.printf("NETWORK: Updating Bus Stop %s...\n", stopId.c_str());
        BusData tempBus;
        if (BusService::updateBusTimes(tempBus, stopId,
                                       NetworkManager::getAppId().c_str(),
                                       NetworkManager::getAppKey().c_str())) {
          busCaches[targetBusIndex].data = tempBus;
          busCaches[targetBusIndex].lastUpdate = now;

          xSemaphoreTake(dataMutex, portMAX_DELAY);
          busData = tempBus;
          busDataUpdated = true;
          xSemaphoreGive(dataMutex);
          GuiController::clearBusStationChanged();
        }
      }
    }

    // ---------------- STOCK ----------------
    if (now - lastStockUpdate > 300000 || manualStockTrigger) {
      manualStockTrigger = false;
      String syms = NetworkManager::getStockSymbols();
      if (syms.length() > 0) {
        Serial.println("NETWORK: Updating Stocks...");
        std::vector<StockItem> items = StockService::getQuotes(syms);
        if (!items.empty()) {
          xSemaphoreTake(dataMutex, portMAX_DELAY);
          stockData = items;
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
