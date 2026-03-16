#include "TouchDrv.h"

TouchDrv::TouchDrv() {}

void TouchDrv::begin() {
  Wire.begin(_sda, _scl);
  Wire.setClock(400000); // 400kHz for fast touch response
  Wire.setTimeOut(20);   // 20ms timeout to prevent blocking loop

  // Int Pin Configuration
  pinMode(_int, INPUT); // Just input

  // Reset Pin Configuration
  Serial.println("TOUCH: Performing Reset Sequence...");
  pinMode(_rst, OUTPUT);
  digitalWrite(_rst, HIGH);
  delay(50);
  digitalWrite(_rst, LOW);
  delay(20);
  digitalWrite(_rst, HIGH);
  delay(200);
  digitalWrite(_rst, LOW);
  delay(20);
  digitalWrite(_rst, HIGH);
  delay(400);

  // Initialize Touch
  digitalWrite(_rst, HIGH);
  delay(100);
  digitalWrite(_rst, LOW);
  delay(50);
  digitalWrite(_rst, HIGH);
  delay(200);

  Wire.beginTransmission(I2C_ADDR_CST820);
  Wire.write(0xFE);
  Wire.write(0xFF); // Disable Auto Sleep
  Wire.endTransmission();
  delay(20);

  Wire.beginTransmission(I2C_ADDR_CST820);
  Wire.write(0xFA);
  Wire.write(0x60); // Threshold?
  Wire.endTransmission();
  delay(20);

  Wire.beginTransmission(I2C_ADDR_CST820);
  Wire.write(0xFE);
  Wire.write(0xFF); // Disable Auto Sleep Again
  Wire.endTransmission();
  delay(20);

  uint8_t id = i2c_read(0xA7);
  Serial.printf("TOUCH: Read Chip ID (0xA7): 0x%02X\n", id);

  uint8_t id2 = i2c_read(0x15);
  Serial.printf("TOUCH: Read Alt ID (0x15): 0x%02X\n", id2);
}

bool TouchDrv::read(int16_t *x, int16_t *y) {
  uint8_t fingerNum = i2c_read(0x02);

  // Debug I2C status every ~100 calls to avoid spam, OR if finger detected
  static int debug_cnt = 0;
  if (fingerNum > 0 || debug_cnt++ > 500) {
    if (debug_cnt > 500)
      debug_cnt = 0;
    // Serial.printf("TOUCH: Reg 0x02 (FingerNum) = %d\n", fingerNum);
  }

  if (fingerNum == 255) {
    Serial.print("!"); // I2C Fail
    return false;
  }

  if (fingerNum == 0) {
    if (touchActive) {
      int16_t dx = lastX - swipeStartX;
      int16_t dy = lastY - swipeStartY;
      int16_t adx = abs(dx);
      int16_t ady = abs(dy);

      const int16_t swipeMin = 35;
      if (adx > swipeMin || ady > swipeMin) {
        if (adx > ady * 1.2f) {
          pendingSwipe = (dx > 0) ? SwipeDirection::Right : SwipeDirection::Left;
        } else if (ady > adx * 1.2f) {
          pendingSwipe = (dy > 0) ? SwipeDirection::Down : SwipeDirection::Up;
        }
      }
      touchActive = false;
      swipeStartX = swipeStartY = -1;
      lastX = lastY = -1;
    }
    return false;
  }

  // Finger is present — read coordinates; bail if I2C fails
  uint8_t data[4] = {0};
  if (!i2c_read_continuous(0x03, data, 4))
    return false;

  int16_t mappedX = ((data[0] & 0x0f) << 8) | data[1];
  int16_t mappedY = ((data[2] & 0x0f) << 8) | data[3];

  // Accept high-speed swipes with large jumps; we use the manual swipe detector.
  if (!touchActive) {
    touchActive = true;
    swipeStartX = mappedX;
    swipeStartY = mappedY;
  }
  lastX = mappedX;
  lastY = mappedY;

  *x = mappedX;
  *y = mappedY;
  return true;
}

TouchDrv::SwipeDirection TouchDrv::consumeSwipe() {
  SwipeDirection result = pendingSwipe;
  pendingSwipe = SwipeDirection::None;
  return result;
}

uint8_t TouchDrv::i2c_read(uint8_t addr) {
  uint8_t rdData = 0;
  uint8_t rdDataCount;
  int retries = 5;
  do {
    Wire.beginTransmission(I2C_ADDR_CST820);
    Wire.write(addr);
    Wire.endTransmission(false); // Restart
    rdDataCount = Wire.requestFrom(I2C_ADDR_CST820, 1);
    if (rdDataCount == 0)
      delay(1);
    retries--;
  } while (rdDataCount == 0 && retries > 0);

  while (Wire.available()) {
    rdData = Wire.read();
  }
  return rdData;
}

bool TouchDrv::i2c_read_continuous(uint8_t addr, uint8_t *data,
                                   uint32_t length) {
  Wire.beginTransmission(I2C_ADDR_CST820);
  Wire.write(addr);
  if (Wire.endTransmission(true))
    return false;

  uint8_t received = Wire.requestFrom(I2C_ADDR_CST820, (size_t)length);
  if (received < length) {
    while (Wire.available())
      Wire.read(); // flush partial data
    memset(data, 0, length);
    return false;
  }
  for (uint32_t i = 0; i < length; i++) {
    *data++ = Wire.read();
  }
  return true;
}

void TouchDrv::i2c_write(uint8_t addr, uint8_t data) {
  Wire.beginTransmission(I2C_ADDR_CST820);
  Wire.write(addr);
  Wire.write(data);
  Wire.endTransmission();
}
