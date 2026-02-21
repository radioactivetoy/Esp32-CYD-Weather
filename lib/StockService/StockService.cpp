#include "StockService.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

std::vector<StockItem> StockService::getQuotes(String symbols) {
  std::vector<StockItem> items;

  // Symbols are comma separated: "AAPL,MSFT,BTC-USD,GRF.MC"
  int startIndex = 0;
  while (startIndex < symbols.length()) {
    int commaIndex = symbols.indexOf(',', startIndex);
    if (commaIndex == -1)
      commaIndex = symbols.length();

    String symbol = symbols.substring(startIndex, commaIndex);
    symbol.trim();
    startIndex = commaIndex + 1;

    if (symbol.isEmpty())
      continue;

    // Fetch
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      // Yahoo Finance Query
      // https://query1.finance.yahoo.com/v8/finance/chart/AAPL?interval=1d&range=1d
      String url = "https://query1.finance.yahoo.com/v8/finance/chart/" +
                   symbol + "?interval=1d&range=1d";

      // Serial.printf("STOCK: Fetching %s\n", symbol.c_str());

      http.begin(url);
      http.setUserAgent("Mozilla/5.0 (esp32)"); // Yahoo blocks generic agents
      int httpCode = http.GET();

      if (httpCode > 0) {
        // Correctly handle HTTP 200 OK
        if (httpCode == HTTP_CODE_OK) {
          JsonDocument doc;
          // Filter data to save memory (Yahoo JSON is huge)
          JsonDocument filter;
          filter["chart"]["result"][0]["meta"] = true; // Capture all meta data

          // STREAM PARSING: Read directly from socket (Low RAM usage)
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
              item.price = price;
              // Calculate Change %
              if (prevClose != 0.0f) {
                item.changePercent = ((price - prevClose) / prevClose) * 100.0f;
              } else {
                item.changePercent = 0.0f;
              }

              item.isValid = true;
              items.push_back(item);
              Serial.printf("STOCK: Parsed %s -> $%.2f (%.2f%%)\n",
                            symbol.c_str(), price, item.changePercent);
            } else {
              Serial.printf("STOCK: Invalid data for %s (Zero Price)\n",
                            symbol.c_str());
            }
          } else {
            Serial.printf("STOCK: JSON Error for %s: %s\n", symbol.c_str(),
                          error.c_str());
          }
        } else {
          Serial.printf("STOCK: HTTP Error for %s: %d\n", symbol.c_str(),
                        httpCode);
        }
      } else {
        Serial.printf("STOCK: Connection Failed for %s\n", symbol.c_str());
      }
      http.end();
    }
  }

  return items;
}
