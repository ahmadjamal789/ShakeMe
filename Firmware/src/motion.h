// motion.h - turns raw accelerometer readings into physics inputs.
//
// Pure C++ (float is fine here: it runs a few times per frame, not per particle).
//
// Physics background: an accelerometer measures "specific force" f = a_device - g.
// A free particle inside the container therefore accelerates, relative to the
// container, by -f. At rest f points UP (reaction to gravity), so -f points down:
// gravity and shaking come out of one formula. FIXED mode keeps gravity pointing
// to the bottom of the screen and uses only the shaking part; TILT mode uses the
// full vector so particles slosh when you tilt the device.
#pragma once
#include "config.h"
#include "particles.h"

class Motion {
 public:
  void init();
  // raw[3]: IMU X,Y,Z in g. valid=false keeps the previous values (sensor error).
  dust::StepInput update(const float raw[3], bool valid);

  void setTiltMode(bool tilt) { tilt_ = tilt; }
  bool tiltMode() const { return tilt_; }

  float agitation() const { return agit_; }   // 0..1
  float linearG() const { return linMag_; }   // shaking strength, g (gravity removed)
  const float* screen() const { return s_; }  // mapped accelerometer, screen axes (g)

 private:
  float s_[3] = {0, -1, 0};
  float grav_[3] = {0, -1, 0};
  float lin_[3] = {0, 0, 0};
  float linMag_ = 0;
  float agit_ = BOOT_AGITATION;
  bool primed_ = false;
  bool tilt_ = false;
};
