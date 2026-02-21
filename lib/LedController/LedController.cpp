#include "LedController.h"
#include "NetworkManager.h"

// CYD RGB LED Pins (Active LOW on many boards, let's assume Active LOW)
// R=4, G=16, B=17
#define PIN_RED 4
#define PIN_GREEN 16
#define PIN_BLUE 17

void LedController::begin() {
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);

  // Turn off (assuming Active LOW, HIGH is off)
  digitalWrite(PIN_RED, HIGH);
  digitalWrite(PIN_GREEN, HIGH);
  digitalWrite(PIN_BLUE, HIGH);

  Serial.println("LED: Test RED...");
  setRGB(255, 0, 0);
  delay(500);
  Serial.println("LED: Test GREEN...");
  setRGB(0, 255, 0);
  delay(500);
  Serial.println("LED: Test BLUE...");
  setRGB(0, 0, 255);
  delay(500);
  Serial.println("LED: Test OFF...");
  setRGB(0, 0, 0);
  delay(200);
}

void LedController::setRGB(uint8_t r, uint8_t g, uint8_t b) {
  // Dimming Logic: Scale down input 0-255 based on preference
  // Active Low: 255 = OFF, 0 = Full ON.
  // We want to map: Input 0 -> 255 (OFF), Input 255 -> 255 - (255/Divisor)

  int divisor = 8; // Default Medium (12%)
  String bright = NetworkManager::getLedBrightness();
  if (bright == "high")
    divisor = 1; // 100%
  else if (bright == "low")
    divisor = 32; // ~3%
  // else medium or unknown -> 8

  int dim_r = r / divisor;
  int dim_g = g / divisor;
  int dim_b = b / divisor;

  analogWrite(PIN_RED, 255 - dim_r);
  analogWrite(PIN_GREEN, 255 - dim_g);
  analogWrite(PIN_BLUE, 255 - dim_b);
}

bool LedController::isRain(int code) { return (code >= 51); }

void LedController::update(const WeatherData &data) {
  Serial.printf("LED: Check Weather Code=%d\n", data.currentWeatherCode);

  // 1. Red: Raining NOW
  if (isRain(data.currentWeatherCode)) {
    Serial.println("LED: Condition RED (Walking in Rain)");
    setRGB(255, 0, 0); // RED
    return;
  }

  // 2. Orange: Rain expected in next 2 hours
  for (int i = 1; i <= 2; i++) {
    if (isRain(data.hourly[i].weatherCode)) {
      Serial.printf("LED: Condition ORANGE (Rain in %dh)\n", i);
      setRGB(255, 60, 0); // ORANGE (Reduced Green for better contrast vs Red)
      return;
    }
  }

  // 3. Blue: Rain expected later
  for (int i = 3; i < 15; i++) {
    if (i >= 24)
      break;
    if (isRain(data.hourly[i].weatherCode)) {
      Serial.println("LED: Condition BLUE (Rain later)");
      setRGB(0, 0, 255); // BLUE (Replaces Yellow for better visibility)
      return;
    }
  }

  // 4. Green: No rain expected
  Serial.println("LED: Condition GREEN (Clear)");
  setRGB(0, 255, 0); // GREEN
}
