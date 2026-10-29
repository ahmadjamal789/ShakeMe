// imu_mpu6050.h - minimal MPU6050 accelerometer driver (pico-sdk I2C, no library).
#pragma once
#include <stddef.h>
#include <stdint.h>

class Mpu6050 {
 public:
  // Returns true if the chip answered. Gyroscope is put in standby (saves power).
  bool init();
  // Reads acceleration in g (IMU axes). Returns false on I2C error.
  bool readAccelG(float out[3]);
  bool ok() const { return ok_; }
  uint32_t errorCount() const { return errors_; }
  uint8_t whoAmI() const { return who_; }

 private:
  bool writeReg(uint8_t reg, uint8_t val);
  bool readRegs(uint8_t reg, uint8_t* dst, size_t n);
  bool ok_ = false;
  uint32_t errors_ = 0;
  uint8_t who_ = 0;
};
