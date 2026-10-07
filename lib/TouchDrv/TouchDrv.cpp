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

  i2c_write(0xFE, 0xFF); // Disable Auto Sleep
  delay(20);
  i2c_write(0xFA, 0x60); // Threshold?
  delay(20);
  i2c_write(0xFE, 0xFF); // Disable Auto Sleep Again
  delay(20);

  uint8_t id = i2c_read(0xA7);
  Serial.printf("TOUCH: Read Chip ID (0xA7): 0x%02X\n", id);

  uint8_t id2 = i2c_read(0x15);
  Serial.printf("TOUCH: Read Alt ID (0x15): 0x%02X\n", id2);
}

// Gestures are detected by LVGL from the raw points reported here.
bool TouchDrv::read(int16_t *x, int16_t *y) {
  uint8_t fingerNum = i2c_read(0x02);

  if (fingerNum == 255) {
    Serial.print("!"); // I2C Fail
    return false;
  }
  if (fingerNum == 0)
    return false;

  // Finger is present — read coordinates; bail if I2C fails
  uint8_t data[4] = {0};
  if (!i2c_read_continuous(0x03, data, 4))
    return false;

  *x = ((data[0] & 0x0f) << 8) | data[1];
  *y = ((data[2] & 0x0f) << 8) | data[3];
  return true;
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
