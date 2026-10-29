#include "motion.h"

#include <math.h>

namespace {
constexpr float DT = 1.0f / (float)PHYS_HZ;
constexpr float ACC_TO_Q12 = (float)dust::ONE / (float)(PHYS_HZ * PHYS_HZ);  // px/s^2 -> Q12 px/step^2
float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
}  // namespace

static float g_decay = 0.99f;
static float g_gravAlpha = 0.01f;

void Motion::init() {
  g_decay = expf(-DT / AGIT_TAU_S);
  g_gravAlpha = DT / (GRAV_LP_TAU_S + DT);
  agit_ = BOOT_AGITATION;
  primed_ = false;
}

dust::StepInput Motion::update(const float raw[3], bool valid) {
  if (valid) {
    s_[0] = MAP_SX_SIGN * raw[MAP_SX_AXIS];
    s_[1] = MAP_SY_SIGN * raw[MAP_SY_AXIS];
    s_[2] = MAP_SZ_SIGN * raw[MAP_SZ_AXIS];
    if (!primed_) {  // start with the gravity estimate already correct
      for (int k = 0; k < 3; ++k) grav_[k] = s_[k];
      primed_ = true;
    }
    // Slow low-pass tracks gravity; whatever is left is the shaking.
    for (int k = 0; k < 3; ++k) {
      grav_[k] += g_gravAlpha * (s_[k] - grav_[k]);
      lin_[k] = s_[k] - grav_[k];
    }
    linMag_ = sqrtf(lin_[0] * lin_[0] + lin_[1] * lin_[1] + lin_[2] * lin_[2]);
  } else {
    linMag_ = 0;
    lin_[0] = lin_[1] = lin_[2] = 0;
  }

  // Agitation: instant attack, exponential decay.
  float target = clamp01((linMag_ - AGIT_DEADBAND_G) / (AGIT_FULL_G - AGIT_DEADBAND_G));
  float decayed = agit_ * g_decay;
  agit_ = decayed > target ? decayed : target;
  if (agit_ < 0.001f) agit_ = 0;

  // Acceleration of the particles relative to the screen, px/s^2.
  float ax, ay;
  if (tilt_) {
    ax = -(grav_[0] * GRAVITY_PX_S2 + lin_[0] * SHAKE_PX_S2_PER_G);
    ay = -(grav_[1] * GRAVITY_PX_S2 + lin_[1] * SHAKE_PX_S2_PER_G);
  } else {
    ax = -lin_[0] * SHAKE_PX_S2_PER_G;
    ay = GRAVITY_PX_S2 - lin_[1] * SHAKE_PX_S2_PER_G;
  }

  dust::StepInput in;
  in.ax = (int32_t)(ax * ACC_TO_Q12);
  in.ay = (int32_t)(ay * ACC_TO_Q12);
  in.kick = agit_ > 0.02f ? (int32_t)(agit_ * KICK_PX_PER_STEP * (float)dust::ONE) : 0;
  return in;
}
