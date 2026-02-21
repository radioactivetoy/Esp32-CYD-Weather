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

  // Always read coordinates for debug
  uint8_t data[4];
  i2c_read_continuous(0x03, data, 4);

  uint16_t _x = ((data[0] & 0x0f) << 8) | data[1];
  uint16_t _y = ((data[2] & 0x0f) << 8) | data[3];

  // Return if no finger
  if (fingerNum == 0) {
    // Serial.print("."); // Alive tick
    return false;
  }
  if (fingerNum == 255) {
    Serial.print("!"); // I2C Fail
    return false;
  }

  // Serial.printf("TOUCH RAW: num=%d, raw_x=%d, raw_y=%d\n", fingerNum, _x,
  // _y);

  // Invert/Map logic for 240x320
  // User reported reverse scroll. Adjusting mapping.
  // Try passing raw Y (or inverted X depending on orientation)
  // Assuming portrait:
  // Debug: Print raw and mapped coordinates
  // Serial.printf("TOUCH DEBUG: Raw X=%d Y=%d -> Mapped X=%d Y=%d\n", _x, _y,
  // _x, _y); Just print mapped for now to see what LVGL gets

  *x = _x;
  *y = _y;

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

uint8_t TouchDrv::i2c_read_continuous(uint8_t addr, uint8_t *data,
                                      uint32_t length) {
  Wire.beginTransmission(I2C_ADDR_CST820);
  Wire.write(addr);
  if (Wire.endTransmission(true))
    return -1;

  Wire.requestFrom(I2C_ADDR_CST820, (size_t)length);
  for (int i = 0; i < length; i++) {
    *data++ = Wire.read();
  }
  return 0;
}

void TouchDrv::i2c_write(uint8_t addr, uint8_t data) {
  Wire.beginTransmission(I2C_ADDR_CST820);
  Wire.write(addr);
  Wire.write(data);
  Wire.endTransmission();
}
