#ifndef TOUCH_DRV_H
#define TOUCH_DRV_H

#include <Arduino.h>
#include <Wire.h>

#define I2C_ADDR_CST820 0x15

class TouchDrv {
public:
  TouchDrv();
  void begin();
  bool read(int16_t *x, int16_t *y);

private:
  uint8_t i2c_read(uint8_t addr);
  uint8_t i2c_read_continuous(uint8_t addr, uint8_t *data, uint32_t length);
  void i2c_write(uint8_t addr, uint8_t data);

  // Hardcoded for this board
  const int8_t _sda = 33;
  const int8_t _scl = 32;
  const int8_t _rst = 25;
  const int8_t _int = 21;
};

#endif
