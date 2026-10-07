#include "BusService.h"
#include <algorithm> // For sort

// TMB API: https://developer.tmb.cat/api-docs/v1/transit
// Endpoint: /ibus/stops/{stopCode}

#include <WiFiClientSecure.h>

bool BusService::updateBusTimes(BusData &data, String stopCode, String appId,
                                String appKey) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  WiFiClientSecure client;
  client.setInsecure(); // Skip SSL verification

  HTTPClient http;

  // Use the combined itransit endpoint
  String url = "https://api.tmb.cat/v1/itransit/bus/parades/" + stopCode +
               "?app_id=" + appId + "&app_key=" + appKey;

  // Serial.println("Fetching Combined Bus Data: " + url);
  http.begin(client, url);
  http.useHTTP10(true);
  http.setConnectTimeout(5000);
  http.setTimeout(5000);

  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
    // Use Stream to save RAM
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      http.end();
      client.stop();
      return false;
    }

    // Capture root timestamp for relative time calculation (ms)
    long long rootTimestamp = doc["timestamp"].as<long long>();
    if (rootTimestamp == 0) {
      Serial.println("BusService: missing root timestamp, discarding response");
      http.end();
      client.stop();
      return false;
    }

    JsonArray parades = doc["parades"];
    if (parades.size() == 0) {
      Serial.println("No parades found. (Valid Response)");
      data.arrivals.clear();
      data.stopCode = stopCode;
      http.end();
      client.stop();
      return true; // Return TRUE so UI updates to show "No Buses"
    }

    JsonObject p = parades[0];
    String stopName = p["nom_parada"].as<String>();
    if (stopName.length() > 0) {
      data.stopName = stopName; // Raw UTF-8; the UI renders accents
    }

    data.arrivals.clear();
    data.stopCode = stopCode;

    JsonArray lines = p["linies_trajectes"];
    for (JsonObject l : lines) {
      String lineName = l["nom_linia"].as<String>();
      // Removed line filter logic

      String destination = l["desti_trajecte"].as<String>();
      JsonArray buses = l["propers_busos"];

      for (JsonObject b : buses) {
        long long arrivalMs = b["temps_arribada"].as<long long>();
        int diffSeconds = (int)((arrivalMs - rootTimestamp) / 1000);

        if (diffSeconds < -30)
          continue; // Skip ghost buses (too old)
        if (diffSeconds < 0)
          diffSeconds = 0; // Arrived?

        BusArrival arr;
        arr.line = lineName;
        arr.destination = destination;
        arr.seconds = diffSeconds; // BusView formats and counts it down

        data.arrivals.push_back(arr);
      }
    }

    // Sort by arrival time
    std::sort(data.arrivals.begin(), data.arrivals.end(),
              [](const BusArrival &a, const BusArrival &b) {
                return a.seconds < b.seconds;
              });

    http.end();
    client.stop();
    // Serial.println("DEBUG: Bus Data Updated (Returning True)");
    return true;
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    http.end();
    client.stop();
    return false;
  }
}
