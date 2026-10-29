#include "imu_mpu6050.h"

#include <Arduino.h>
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

namespace {
constexpr uint8_t REG_SMPLRT_DIV = 0x19;
constexpr uint8_t REG_CONFIG = 0x1A;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t REG_PWR_MGMT_2 = 0x6C;
constexpr uint8_t REG_WHO_AM_I = 0x75;
constexpr uint32_t I2C_TIMEOUT_US = 3000;
constexpr float LSB_PER_G = 4096.0f;  // +/-8 g range
}  // namespace

bool Mpu6050::writeReg(uint8_t reg, uint8_t val) {
  uint8_t buf[2] = {reg, val};
  return i2c_write_timeout_us(IMU_I2C_PORT, IMU_I2C_ADDR, buf, 2, false, I2C_TIMEOUT_US) == 2;
}

bool Mpu6050::readRegs(uint8_t reg, uint8_t* dst, size_t n) {
  if (i2c_write_timeout_us(IMU_I2C_PORT, IMU_I2C_ADDR, &reg, 1, true, I2C_TIMEOUT_US) != 1) return false;
  return i2c_read_timeout_us(IMU_I2C_PORT, IMU_I2C_ADDR, dst, n, false, I2C_TIMEOUT_US) == (int)n;
}

bool Mpu6050::init() {
  i2c_init(IMU_I2C_PORT, 400000);
  gpio_set_function(IMU_PIN_SDA, GPIO_FUNC_I2C);
  gpio_set_function(IMU_PIN_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(IMU_PIN_SDA);
  gpio_pull_up(IMU_PIN_SCL);
  delay(50);  // let the sensor power up

  ok_ = false;
  if (!readRegs(REG_WHO_AM_I, &who_, 1)) return false;
  // 0x68 for a genuine MPU6050 (the value does not depend on AD0). Clones report
  // other values (0x70, 0x71, 0x72 ...) but behave the same, so we only warn.
  if (!writeReg(REG_PWR_MGMT_1, 0x80)) return false;  // device reset
  delay(100);
  if (!writeReg(REG_PWR_MGMT_1, 0x00)) return false;  // wake, internal 8 MHz clock
  delay(10);
  bool good = true;
  good &= writeReg(REG_PWR_MGMT_2, 0x07);    // gyro axes in standby (accel only)
  good &= writeReg(REG_SMPLRT_DIV, 0x00);    // 1 kHz sample rate
  good &= writeReg(REG_CONFIG, 0x03);        // digital low-pass ~44 Hz
  good &= writeReg(REG_ACCEL_CONFIG, 0x10);  // +/-8 g
  ok_ = good;
  return good;
}

bool Mpu6050::readAccelG(float out[3]) {
  uint8_t b[6];
  if (!readRegs(REG_ACCEL_XOUT_H, b, 6)) {
    ++errors_;
    ok_ = false;
    return false;
  }
  ok_ = true;
  for (int k = 0; k < 3; ++k) {
    int16_t raw = (int16_t)((b[2 * k] << 8) | b[2 * k + 1]);
    out[k] = (float)raw / LSB_PER_G;
  }
  return true;
}
