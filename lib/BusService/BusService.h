#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <vector>

struct BusArrival {
  String line;
  String destination;
  String text; // e.g. "5 min"
  int seconds; // Time in seconds
};

struct BusData {
  String stopName;
  String stopCode;
  std::vector<BusArrival> arrivals;
  uint32_t lastUpdate = 0;
};

class BusService {
public:
  // Returns true if successful. Populates data.
  // stopCode: e.g. "2543"
  // lineFilter: e.g. "V15". If empty, returns all lines.
  static bool updateBusTimes(BusData &data, String stopCode, String appId,
                             String appKey);
};
