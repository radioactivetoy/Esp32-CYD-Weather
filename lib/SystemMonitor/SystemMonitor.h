#pragma once

#include <Arduino.h>

// Heap and task-stack health. Stack figures are the *minimum ever free*
// (FreeRTOS high-water mark), so a value that keeps shrinking towards zero
// warns about an overflow before it happens.
namespace SystemMonitor {

struct Stats {
  uint32_t uptimeS;
  uint32_t heapFree;
  uint32_t heapMinFree;   // Lowest free heap since boot
  uint32_t heapLargest;   // Largest allocatable block (fragmentation)
  int32_t loopStackFree;  // Bytes, -1 if unknown
  int32_t netStackFree;   // Bytes, -1 if unknown
  int rssi;               // dBm, 0 if not connected
};

Stats read();

// Logs a status line every LOG_INTERVAL_MS and warns when a limit is close.
// Call from loop().
void logPeriodic();

String formatUptime(uint32_t seconds); // "3d 04:12"

} // namespace SystemMonitor
