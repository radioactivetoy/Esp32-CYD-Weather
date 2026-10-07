#include "StockService.h"
#include "NetUtils.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

std::vector<StockItem> StockService::getQuotes(const String &symbols) {
  std::vector<StockItem> items;

  // Symbols are comma separated: "AAPL,MSFT,BTC-USD,GRF.MC"
  for (const String &symbol : NetUtils::splitList(symbols, ',', 20)) {
    if (WiFi.status() != WL_CONNECTED)
      break;

    // Each symbol is a separate blocking HTTPS round trip; keep the network
    // task's watchdog happy between them.
    esp_task_wdt_reset();

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    // https://query1.finance.yahoo.com/v8/finance/chart/AAPL?interval=1d&range=1d
    String url = "https://query1.finance.yahoo.com/v8/finance/chart/" +
                 NetUtils::urlEncode(symbol) + "?interval=1d&range=1d";

    http.begin(client, url);
    http.useHTTP10(true); // No chunked encoding: we parse from the stream
    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    http.setUserAgent("Mozilla/5.0 (esp32)"); // Yahoo blocks generic agents
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      // Filter data to save memory (Yahoo JSON is huge)
      JsonDocument filter;
      filter["chart"]["result"][0]["meta"]["regularMarketPrice"] = true;
      filter["chart"]["result"][0]["meta"]["previousClose"] = true;
      filter["chart"]["result"][0]["meta"]["chartPreviousClose"] = true;
      filter["chart"]["result"][0]["meta"]["currency"] = true;

      JsonDocument doc;
      DeserializationError error = deserializeJson(
          doc, http.getStream(), DeserializationOption::Filter(filter));

      if (!error) {
        JsonObject meta = doc["chart"]["result"][0]["meta"];
        float price = meta["regularMarketPrice"];
        float prevClose = meta["previousClose"];

        // Fallback for some assets
        if (prevClose == 0.0f)
          prevClose = meta["chartPreviousClose"];

        if (price != 0.0f) {
          StockItem item;
          item.symbol = symbol;
          item.currency = meta["currency"] | "";
          item.price = price;
          item.changePercent =
              (prevClose != 0.0f) ? ((price - prevClose) / prevClose) * 100.0f
                                  : 0.0f;
          item.isValid = true;
          items.push_back(item);
          Serial.printf("STOCK: Parsed %s -> %.2f %s (%.2f%%)\n",
                        symbol.c_str(), price, item.currency.c_str(),
                        item.changePercent);
        } else {
          Serial.printf("STOCK: Invalid data for %s (Zero Price)\n",
                        symbol.c_str());
        }
      } else {
        Serial.printf("STOCK: JSON Error for %s: %s\n", symbol.c_str(),
                      error.c_str());
      }
    } else if (httpCode > 0) {
      Serial.printf("STOCK: HTTP Error for %s: %d\n", symbol.c_str(), httpCode);
    } else {
      Serial.printf("STOCK: Connection Failed for %s\n", symbol.c_str());
    }
    http.end();
    client.stop();
  }

  return items;
}
