#include "SystemMonitor.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace SystemMonitor {

static const uint32_t LOG_INTERVAL_MS = 60000;
static const uint32_t STACK_WARN_BYTES = 1536;  // TLS alone needs ~3KB
static const uint32_t HEAP_WARN_BYTES = 40000;  // Below this TLS may fail
static const uint32_t BLOCK_WARN_BYTES = 20000; // Fragmentation

// Task handles are looked up by name once and cached.
static int32_t stackFree(const char *taskName, TaskHandle_t &cache) {
  if (!cache)
    cache = xTaskGetHandle(taskName);
  if (!cache)
    return -1;
  // ESP-IDF reports the high-water mark in bytes
  return (int32_t)uxTaskGetStackHighWaterMark(cache);
}

Stats read() {
  static TaskHandle_t loopTask = nullptr;
  static TaskHandle_t netTask = nullptr;

  Stats s;
  s.uptimeS = millis() / 1000;
  s.heapFree = ESP.getFreeHeap();
  s.heapMinFree = ESP.getMinFreeHeap();
  s.heapLargest = ESP.getMaxAllocHeap();
  s.loopStackFree = stackFree("loopTask", loopTask);
  s.netStackFree = stackFree("NetTask", netTask);
  s.rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
  return s;
}

String formatUptime(uint32_t seconds) {
  char buf[24];
  uint32_t days = seconds / 86400;
  uint32_t hours = (seconds % 86400) / 3600;
  uint32_t mins = (seconds % 3600) / 60;
  if (days > 0)
    snprintf(buf, sizeof(buf), "%ud %02u:%02u", days, hours, mins);
  else
    snprintf(buf, sizeof(buf), "%02u:%02u", hours, mins);
  return String(buf);
}

void logPeriodic() {
  static uint32_t lastLog = 0;
  uint32_t now = millis();
  if (lastLog != 0 && now - lastLog < LOG_INTERVAL_MS)
    return;
  lastLog = now ? now : 1;

  Stats s = read();
  Serial.printf("MON: up %s | heap %u (min %u, largest %u) | stack free "
                "loop %d, net %d | RSSI %d\n",
                formatUptime(s.uptimeS).c_str(), s.heapFree, s.heapMinFree,
                s.heapLargest, s.loopStackFree, s.netStackFree, s.rssi);

  if (s.loopStackFree >= 0 && (uint32_t)s.loopStackFree < STACK_WARN_BYTES)
    Serial.printf("MON: WARNING loopTask stack low (%d bytes left)\n",
                  s.loopStackFree);
  if (s.netStackFree >= 0 && (uint32_t)s.netStackFree < STACK_WARN_BYTES)
    Serial.printf("MON: WARNING NetTask stack low (%d bytes left)\n",
                  s.netStackFree);
  if (s.heapMinFree < HEAP_WARN_BYTES)
    Serial.printf("MON: WARNING heap dipped to %u bytes\n", s.heapMinFree);
  if (s.heapLargest < BLOCK_WARN_BYTES)
    Serial.printf("MON: WARNING heap fragmented (largest block %u)\n",
                  s.heapLargest);
}

} // namespace SystemMonitor
