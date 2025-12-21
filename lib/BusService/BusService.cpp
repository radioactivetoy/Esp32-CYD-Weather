#include "BusService.h"
#include <algorithm> // For sort

// Helper to remove Latin chars for display compatibility
String sanitize(String input) {
  input.replace("á", "a");
  input.replace("à", "a");
  input.replace("Á", "A");
  input.replace("À", "A");
  input.replace("é", "e");
  input.replace("è", "e");
  input.replace("É", "E");
  input.replace("È", "E");
  input.replace("í", "i");
  input.replace("Í", "I");
  input.replace("ï", "i");
  input.replace("Ï", "I");
  input.replace("ó", "o");
  input.replace("ò", "o");
  input.replace("Ó", "O");
  input.replace("Ò", "O");
  input.replace("ú", "u");
  input.replace("ù", "u");
  input.replace("ü", "u");
  input.replace("Ú", "U");
  input.replace("Ü", "U");
  input.replace("ñ", "n");
  input.replace("Ñ", "N");
  input.replace("ç", "c");
  input.replace("Ç", "C");
  input.replace("·", ".");
  return input;
}

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
  http.setConnectTimeout(5000);
  http.setTimeout(5000);

  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      http.end();
      return false;
    }

    // Capture root timestamp for relative time calculation (ms)
    long long rootTimestamp = doc["timestamp"].as<long long>();

    JsonArray parades = doc["parades"];
    if (parades.size() == 0) {
      Serial.println("No parades found. (Valid Response)");
      data.arrivals.clear();
      data.stopCode = stopCode;
      http.end();
      return true; // Return TRUE so UI updates to show "No Buses"
    }

    JsonObject p = parades[0];
    String stopName = p["nom_parada"].as<String>();
    if (stopName.length() > 0) {
      data.stopName = sanitize(stopName);
      // Serial.println("Stop Name (Updated): " + data.stopName);
    }

    data.arrivals.clear();
    data.stopCode = stopCode;

    JsonArray lines = p["linies_trajectes"];
    for (JsonObject l : lines) {
      String lineName = l["nom_linia"].as<String>();
      // Removed line filter logic

      String destination = sanitize(l["desti_trajecte"].as<String>());
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
        arr.seconds = diffSeconds;

        // Create text representation
        if (diffSeconds < 60) {
          arr.text = "Prop";
        } else {
          arr.text = String(diffSeconds / 60) + " min";
        }

        data.arrivals.push_back(arr);
      }
    }

    // Sort by arrival time
    std::sort(data.arrivals.begin(), data.arrivals.end(),
              [](const BusArrival &a, const BusArrival &b) {
                return a.seconds < b.seconds;
              });

    http.end();
    // Serial.println("DEBUG: Bus Data Updated (Returning True)");
    return true;
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
}
