#include "DataManager.h"
#include "GuiController.h"
#include "LedController.h"
#include "NetworkManager.h"
#include <esp_task_wdt.h> // Hardware Watchdog

// Task watchdog timeout for the network task. Generous on purpose: a single
// HTTPS call can block for several seconds (TLS handshake + 5s timeouts) and a
// stock refresh chains one call per symbol (StockService feeds it in between).
static const uint32_t NET_WDT_TIMEOUT_S = 60;

static const uint32_t MIN_REQUEST_GAP_MS = 1000;           // Global rate limit
static const uint32_t WEATHER_REFRESH_MS = 15UL * 60000UL; // Background refresh
static const uint32_t WEATHER_SWITCH_STALE_MS = 10UL * 60000UL; // Refetch on switch
static const uint32_t BUS_REFRESH_MS = 60000UL;
static const uint32_t STOCK_REFRESH_MS = 5UL * 60000UL;

// Wrap-safe "now has reached deadline"
static bool timeReached(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}

// Retry delay after consecutive failures: 30s, 1m, 2m, 4m, 8m, then 15m.
static uint32_t backoffMs(uint8_t failCount) {
  uint8_t shift = failCount > 0 ? failCount - 1 : 0;
  if (shift > 5)
    shift = 5;
  uint32_t ms = 30000UL << shift;
  return ms > 900000UL ? 900000UL : ms;
}

// Large structs used only by the network task, kept OFF its stack: a
// WeatherData is ~1KB, and the TLS handshake underneath each request needs
// several KB of stack on its own (NetTask has 10KB in total).
static WeatherData s_fetchWeather;
static const WeatherData s_emptyWeather;

// millis() timestamp that is never 0 (0 means "never updated" in the caches).
static uint32_t stamp() {
  uint32_t m = millis();
  return m ? m : 1;
}

// Defines
SemaphoreHandle_t DataManager::dataMutex = NULL;

WeatherData DataManager::weatherData;
BusData DataManager::busData;
std::vector<StockItem> DataManager::stockData;

std::atomic<bool> DataManager::weatherDataUpdated{false};
std::atomic<bool> DataManager::busDataUpdated{false};
std::atomic<bool> DataManager::stockDataUpdated{false};

std::atomic<int> DataManager::currentUpdatingCityIndex{-1};
std::atomic<int> DataManager::currentUpdatingBusIndex{-1};
std::atomic<bool> DataManager::isUpdatingStock{false};
std::atomic<uint32_t> DataManager::stockLastUpdateTime{0};

std::atomic<bool> DataManager::weatherStatusChanged{false};
std::atomic<bool> DataManager::busStatusChanged{false};

std::atomic<bool> DataManager::manualBusTrigger{false};

std::vector<CityWeatherCache> DataManager::cityCaches;
std::vector<BusStopCache> DataManager::busCaches;

bool DataManager::isWeatherUpdating(int cityIndex) {
  return currentUpdatingCityIndex == cityIndex;
}
bool DataManager::isBusUpdating(int busIndex) {
  return currentUpdatingBusIndex == busIndex;
}
bool DataManager::isStockUpdating() { return isUpdatingStock; }
uint32_t DataManager::getStockLastUpdate() { return stockLastUpdateTime; }

bool DataManager::getWeatherStatusChanged() {
  return weatherStatusChanged.exchange(false);
}

bool DataManager::getBusStatusChanged() {
  return busStatusChanged.exchange(false);
}

void DataManager::begin() {
  dataMutex = xSemaphoreCreateMutex();

  // Start Background Task
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

void DataManager::triggerBusUpdate() { manualBusTrigger = true; }

// --- BACKGROUND TASK (The "Brain") ---
void DataManager::networkTask(void *parameter) {
  while (dataMutex == NULL)
    vTaskDelay(10);

  // GuiController::showLoadingScreen only queues the message under its own
  // mutex; the GUI task draws it.
  GuiController::showLoadingScreen("Connecting WiFi...");
  NetworkManager::begin();

  // Subscribe to the task watchdog only now: the WiFi config portal above may
  // legitimately block for minutes.
  esp_task_wdt_init(NET_WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  GuiController::showLoadingScreen("Fetching Weather...");

  // --- INITIAL SETUP --- (config only changes via the web UI, which reboots)
  std::vector<String> cities = NetworkManager::getCities();
  GuiController::setCityCount(cities.size());
  cityCaches.resize(cities.size());
  for (size_t i = 0; i < cities.size(); i++)
    cityCaches[i].cityName = cities[i];

  std::vector<String> stopIds = NetworkManager::getBusStops();
  GuiController::setBusStopCount(stopIds.size());
  busCaches.resize(stopIds.size());
  for (size_t i = 0; i < stopIds.size(); i++)
    busCaches[i].id = stopIds[i];

  const String owmKey = NetworkManager::getOwmApiKey();
  const String appId = NetworkManager::getAppId();
  const String appKey = NetworkManager::getAppKey();
  const String stockSymbols = NetworkManager::getStockSymbols();

  // --- PUBLISHING (network task -> UI) ---

  // Publish a city to the UI. Without data yet, publish a placeholder that
  // only carries the name, so the UI never shows another city's numbers.
  auto publishCity = [](int idx) {
    const CityWeatherCache &c = cityCaches[idx];
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    if (c.hasData) {
      weatherData = c.data;
    } else {
      weatherData = s_emptyWeather;
      weatherData.cityName =
          c.resolvedName.length() > 0 ? c.resolvedName : c.cityName;
    }
    weatherDataUpdated = true;
    xSemaphoreGive(dataMutex);
  };

  auto publishBus = [](int idx) {
    const BusStopCache &b = busCaches[idx];
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    if (b.hasData) {
      busData = b.data;
    } else {
      busData = BusData();
      busData.stopCode = b.id;
    }
    busDataUpdated = true;
    xSemaphoreGive(dataMutex);
  };

  // --- FETCHING (each performs network requests) ---

  auto fetchCity = [&](int idx) {
    CityWeatherCache &c = cityCaches[idx];
    Serial.printf("NETWORK: Updating City %d: %s\n", idx, c.cityName.c_str());

    currentUpdatingCityIndex = idx;
    if (idx == GuiController::getCityIndex()) {
      weatherStatusChanged = true;    // Signal UI
      vTaskDelay(pdMS_TO_TICKS(50)); // Let the UI paint the yellow dot
    }

    bool ok = true;
    if (!c.hasCoords) {
      ok = WeatherService::lookupCoordinates(c.cityName, c.lat, c.lon,
                                             c.resolvedName, owmKey);
      c.hasCoords = ok;
      esp_task_wdt_reset();
    }

    WeatherData &temp = s_fetchWeather; // Static, see s_fetchWeather above
    temp = s_emptyWeather;              // Fresh defaults for every fetch
    if (ok)
      ok = WeatherService::updateWeather(temp, c.lat, c.lon, owmKey);
    currentUpdatingCityIndex = -1;

    uint32_t t = stamp();
    if (ok) {
      temp.cityName = c.resolvedName.length() > 0 ? c.resolvedName : c.cityName;
      temp.lastUpdate = t;
      c.data = temp;
      c.lastUpdate = t;
      c.hasData = true;
      c.failCount = 0;
      Serial.println("NETWORK: Weather Update Success");
      if (idx == 0)
        LedController::update(temp); // LED tracks the primary city
    } else {
      if (c.failCount < 255)
        c.failCount++;
      c.nextAttempt = t + backoffMs(c.failCount);
      Serial.printf("NETWORK: Weather Update Failed, retry in %us\n",
                    backoffMs(c.failCount) / 1000);
    }

    // Publish if visible, also on failure: clears the yellow dot and, on
    // boot, replaces the loading screen with the (placeholder) weather screen.
    if (idx == GuiController::getCityIndex())
      publishCity(idx);
  };

  auto fetchBus = [&](int idx) {
    BusStopCache &b = busCaches[idx];
    Serial.printf("NETWORK: Updating Bus Stop %s...\n", b.id.c_str());

    currentUpdatingBusIndex = idx;
    if (idx == GuiController::getBusIndex()) {
      busStatusChanged = true;        // Signal UI
      vTaskDelay(pdMS_TO_TICKS(50)); // Let the UI paint the yellow dot
    }

    BusData temp;
    bool ok = BusService::updateBusTimes(temp, b.id, appId, appKey);
    currentUpdatingBusIndex = -1;

    uint32_t t = stamp();
    if (ok) {
      // An empty "no buses" response carries no stop name; keep the old one.
      if (temp.stopName.isEmpty())
        temp.stopName = b.data.stopName;
      temp.lastUpdate = t;
      b.data = temp;
      b.lastUpdate = t;
      b.hasData = true;
      b.failCount = 0;
      Serial.println("NETWORK: Bus Update Success");
    } else {
      if (b.failCount < 255)
        b.failCount++;
      b.nextAttempt = t + backoffMs(b.failCount);
      Serial.printf("NETWORK: Bus Update Failed, retry in %us\n",
                    backoffMs(b.failCount) / 1000);
    }

    if (idx == GuiController::getBusIndex())
      publishBus(idx); // Also on failure, to clear the yellow dot
  };

  uint32_t lastStockUpdate = 0;
  bool stocksFetchedOnce = false;

  auto fetchStocks = [&]() {
    Serial.println("NETWORK: Updating Stocks...");
    isUpdatingStock = true;
    std::vector<StockItem> items = StockService::getQuotes(stockSymbols);
    isUpdatingStock = false;

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    if (!items.empty()) {
      stockData = items;
      stockLastUpdateTime = stamp();
    }
    stockDataUpdated = true; // Also on failure, to refresh the status
    xSemaphoreGive(dataMutex);

    lastStockUpdate = millis();
    stocksFetchedOnce = true;
  };

  // --- SCHEDULING ---

  auto dueCity = [](uint32_t now) -> int {
    for (size_t i = 0; i < cityCaches.size(); i++) {
      const CityWeatherCache &c = cityCaches[i];
      bool stale = !c.hasData || now - c.lastUpdate > WEATHER_REFRESH_MS;
      if (stale && (c.failCount == 0 || timeReached(now, c.nextAttempt)))
        return i;
    }
    return -1;
  };

  auto dueBus = [](uint32_t now) -> int {
    for (size_t i = 0; i < busCaches.size(); i++) {
      const BusStopCache &b = busCaches[i];
      bool stale = !b.hasData || now - b.lastUpdate > BUS_REFRESH_MS;
      if (stale && (b.failCount == 0 || timeReached(now, b.nextAttempt)))
        return i;
    }
    return -1;
  };

  // User-driven requests jump the queue (and ignore backoff once)
  int priorityCity = -1;
  int priorityBus = -1;
  uint32_t lastRequestMs = 0;
  bool requestedOnce = false;

  // --- MAIN LOOP ---
  for (;;) {
    esp_task_wdt_reset();
    NetworkManager::handleClient();
    uint32_t now = millis();

    if (ESP.getFreeHeap() < 65000) {
      Serial.printf("NETWORK: low heap %d, delaying updates\n",
                    ESP.getFreeHeap());
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    // 1. UI switches. Cached data is published right away (no network
    //    needed), so a switch is never lost to the rate limiter.
    int targetCity = GuiController::getCityIndex();
    if (GuiController::consumeCityChanged() && targetCity >= 0 &&
        targetCity < (int)cityCaches.size()) {
      publishCity(targetCity);
      const CityWeatherCache &c = cityCaches[targetCity];
      if (!c.hasData || now - c.lastUpdate > WEATHER_SWITCH_STALE_MS)
        priorityCity = targetCity;
    }

    int targetBus = GuiController::getBusIndex();
    bool busValid = targetBus >= 0 && targetBus < (int)busCaches.size();
    if (GuiController::consumeBusStationChanged() && busValid) {
      publishBus(targetBus);
      const BusStopCache &b = busCaches[targetBus];
      if (!b.hasData || now - b.lastUpdate > BUS_REFRESH_MS)
        priorityBus = targetBus;
    }
    if (manualBusTrigger.exchange(false) && busValid)
      priorityBus = targetBus;

    // 2. At most one network request per MIN_REQUEST_GAP_MS.
    if (!requestedOnce || now - lastRequestMs >= MIN_REQUEST_GAP_MS) {
      bool requested = true;
      int idx;
      if (priorityCity >= 0) {
        idx = priorityCity;
        priorityCity = -1;
        fetchCity(idx);
      } else if (priorityBus >= 0) {
        idx = priorityBus;
        priorityBus = -1;
        fetchBus(idx);
      } else if ((idx = dueCity(now)) >= 0) {
        fetchCity(idx);
      } else if ((idx = dueBus(now)) >= 0) {
        fetchBus(idx);
      } else if (stockSymbols.length() > 0 &&
                 (!stocksFetchedOnce ||
                  now - lastStockUpdate > STOCK_REFRESH_MS)) {
        fetchStocks();
      } else {
        requested = false;
      }

      if (requested) {
        lastRequestMs = millis(); // Gap counts from the end of the request
        requestedOnce = true;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
