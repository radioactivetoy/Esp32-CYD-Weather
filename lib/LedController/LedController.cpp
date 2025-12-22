#include "LedController.h"

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
  // Invert for Active LOW (0 = ON, 255 = OFF)
  // Using digitalWrite for clean ON/OFF to rule out PWM issues on IO16/17

  if (r == 255)
    digitalWrite(PIN_RED, LOW);
  else if (r == 0)
    digitalWrite(PIN_RED, HIGH);
  else
    analogWrite(PIN_RED, 255 - r);

  if (g == 255)
    digitalWrite(PIN_GREEN, LOW);
  else if (g == 0)
    digitalWrite(PIN_GREEN, HIGH);
  else
    analogWrite(PIN_GREEN, 255 - g);

  if (b == 255)
    digitalWrite(PIN_BLUE, LOW);
  else if (b == 0)
    digitalWrite(PIN_BLUE, HIGH);
  else
    analogWrite(PIN_BLUE, 255 - b);
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
      setRGB(255, 165, 0); // ORANGE
      return;
    }
  }

  // 3. Yellow: Rain expected later
  for (int i = 3; i < 15; i++) {
    if (i >= 24)
      break;
    if (isRain(data.hourly[i].weatherCode)) {
      Serial.println("LED: Condition YELLOW (Rain later)");
      setRGB(255, 255, 0); // YELLOW
      return;
    }
  }

  // 4. Green: No rain expected
  Serial.println("LED: Condition GREEN (Clear)");
  setRGB(0, 255, 0); // GREEN
}
