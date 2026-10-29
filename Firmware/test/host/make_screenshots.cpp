// Runs the real engine + motion + renderer on a PC with a simulated accelerometer
// and writes PPM screenshots (converted to PNG by make_screenshots.py).
#include <math.h>
#include <stdio.h>
#include <string>
#include "motion.h"
#include "particles.h"
#include "render.h"

static uint16_t fb[SCREEN_W * SCREEN_H];

static void savePPM(const char* name) {
  FILE* f = fopen(name, "wb");
  fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
  for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
    uint16_t v = fb[i];
    uint16_t c = (uint16_t)((v >> 8) | (v << 8));  // undo wire byte swap
    uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

int main() {
  static dust::Engine eng;
  Motion motion;
  gfx::GraphHistory graph;
  eng.init(777);
  motion.init();
  gfx::init(eng);
  gfx::Canvas cv{fb};

  float t = 0;
  auto stepFor = [&](float seconds, float shakeG, float hz) {
    int n = (int)(seconds * PHYS_HZ);
    for (int k = 0; k < n; ++k, t += 1.0f / PHYS_HZ) {
      float sx = shakeG * sinf(2 * 3.14159265f * hz * t);
      float raw[3] = {sx, 1.0f, 0.02f};   // upright device: IMU Y reads +1 g (maps to screen y = -1)
      eng.step(motion.update(raw, true));
      if (k % 4 == 0) graph.push(motion.linearG(), motion.agitation());
    }
  };
  auto shot = [&](const char* name, bool graphView) {
    if (graphView) gfx::drawGraph(cv, graph); else gfx::drawBeaker(cv, eng);
    gfx::drawBattery(cv, 72, false);
    savePPM(name);
  };

  stepFor(0.3f, 0, 0);   shot("/tmp/shot_1_boot.ppm", false);
  stepFor(1.2f, 0, 0);   shot("/tmp/shot_2_falling.ppm", false);
  stepFor(8.0f, 0, 0);   shot("/tmp/shot_3_settled.ppm", false);
  stepFor(1.0f, 0.6f, 4); shot("/tmp/shot_4_gentle.ppm", false);
  stepFor(8.0f, 0, 0);
  stepFor(1.4f, 4.0f, 5); shot("/tmp/shot_5_violent.ppm", false);
  stepFor(2.5f, 0, 0);   shot("/tmp/shot_6_decaying.ppm", false);
  shot("/tmp/shot_8_graph.ppm", true);
  stepFor(10.0f, 0, 0);  shot("/tmp/shot_7_resettled.ppm", false);
  printf("screenshots written\n");
  return 0;
}
