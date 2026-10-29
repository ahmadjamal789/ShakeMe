// Host-side physics test: runs the real engine on a PC and prints metrics.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#include "particles.h"

using namespace dust;

static int32_t accQ12(float px_s2) { return (int32_t)(px_s2 * (float)ONE / (float)(PHYS_HZ * PHYS_HZ)); }

struct Stats { float meanSpeed, topFrac, maxVy; int outOfBounds; float minDistOverR; };

static Stats measure(const Engine& e, bool pairs) {
  Stats s{};
  int n = e.count();
  std::vector<int32_t> ys(n);
  double sum = 0;
  for (int i = 0; i < n; ++i) {
    double sp = sqrt((double)e.vx()[i] * e.vx()[i] + (double)e.vy()[i] * e.vy()[i]) * PHYS_HZ / ONE;
    sum += sp;
    ys[i] = e.y()[i];
    if (e.x()[i] < e.radiusQ12() || e.x()[i] > SCREEN_W * ONE - e.radiusQ12() ||
        e.y()[i] < e.radiusQ12() || e.y()[i] > SCREEN_H * ONE - e.radiusQ12())
      s.outOfBounds++;
  }
  s.meanSpeed = (float)(sum / n);
  std::sort(ys.begin(), ys.end());
  float top = (float)ys[(int)(n * 0.03)] / ONE - e.radiusPx();   // 3rd percentile ignores stragglers
  s.topFrac = (SCREEN_H - top) / SCREEN_H;
  s.minDistOverR = 99;
  if (pairs) {
    double best = 1e18;
    for (int i = 0; i < n; ++i)
      for (int j = i + 1; j < n; ++j) {
        double dx = (double)(e.x()[i] - e.x()[j]) / ONE, dy = (double)(e.y()[i] - e.y()[j]) / ONE;
        double d = dx * dx + dy * dy;
        if (d < best) best = d;
      }
    s.minDistOverR = (float)(sqrt(best) / (2.0 * e.radiusPx()));   // 1.0 = just touching
  }
  return s;
}

static float agitFromShake(float amplitudeG) {   // same mapping as motion.cpp
  float t = (amplitudeG - AGIT_DEADBAND_G) / (AGIT_FULL_G - AGIT_DEADBAND_G);
  return t < 0 ? 0 : (t > 1 ? 1 : t);
}

int main() {
  static Engine e;
  e.init(12345);
  printf("particles=%d radius=%.2f px  step=%d Hz  drag_div=%d\n", PARTICLE_COUNT, e.radiusPx(), PHYS_HZ, (int)((float)PHYS_HZ / DRAG_PER_S + 0.5f));

  const float decay = expf(-1.0f / (AGIT_TAU_S * PHYS_HZ));
  float E = BOOT_AGITATION;
  StepInput in;
  int fails = 0;
  uint64_t totalChecks = 0, totalContacts = 0; long steps = 0;

  auto run = [&](const char* label, float seconds, float shakeG, float shakeHz, bool shaking) {
    printf("\n--- %s ---\n  t(s)  speed(px/s)  pile_top(%% screen)  min_dist/diam  out_of_bounds\n", label);
    int n = (int)(seconds * PHYS_HZ);
    for (int k = 0; k < n; ++k) {
      float t = (float)k / PHYS_HZ;
      float ax_px = 0;
      float target = 0;
      if (shaking) {
        float a_g = shakeG * sinf(2.0f * 3.14159265f * shakeHz * t);
        ax_px = -a_g * SHAKE_PX_S2_PER_G;
        target = agitFromShake(fabsf(shakeG) * 0.7f);
      }
      E = fmaxf(E * decay, target);
      in.ax = accQ12(ax_px);
      in.ay = accQ12(GRAVITY_PX_S2);
      in.kick = E > 0.02f ? (int32_t)(E * KICK_PX_PER_STEP * ONE) : 0;
      uint32_t c0 = e.pairChecks, k0 = e.pairContacts;
      e.step(in);
      totalChecks += e.pairChecks - c0; totalContacts += e.pairContacts - k0; steps++;
      if (k % (PHYS_HZ / 2) == 0) {
        bool pairs = (k % PHYS_HZ == 0);
        Stats s = measure(e, pairs);
        char buf[16];
        if (pairs) snprintf(buf, sizeof buf, "%.2f", s.minDistOverR); else snprintf(buf, sizeof buf, "-");
        printf("  %5.1f  %9.1f   %9.1f        %8s      %d\n", t, s.meanSpeed, s.topFrac * 100, buf, s.outOfBounds);
        if (s.outOfBounds) fails++;
      }
    }
  };

  run("boot: settle from suspended state", 16, 0, 0, false);
  Stats settled = measure(e, true);
  printf("\nSETTLED: pile top = %.1f%% of screen height (target %.0f%%), mean speed %.2f px/s, min dist/diam %.2f\n",
         settled.topFrac * 100, SETTLED_FRACTION * 100, settled.meanSpeed, settled.minDistOverR);
  run("gentle shake 0.5 g @ 4 Hz for 1.5 s, then rest", 1.5f, 0.5f, 4.0f, true);
  run("recover from gentle", 8, 0, 0, false);
  run("violent shake 4 g @ 5 Hz for 2 s", 2.0f, 4.0f, 5.0f, true);
  run("recover from violent shake (energy dies off, particles resettle)", 14, 0, 0, false);
  Stats after = measure(e, true);
  printf("\nAFTER VIOLENT SHAKE + REST: pile top = %.1f%%, mean speed %.2f px/s, min dist/diam %.2f\n",
         after.topFrac * 100, after.meanSpeed, after.minDistOverR);
  printf("avg pair checks/step = %.0f, avg contacts/step = %.0f\n", (double)totalChecks / steps, (double)totalContacts / steps);
  if (fails) printf("\nFAIL: %d out-of-bounds samples\n", fails);
  return fails ? 1 : 0;
}
