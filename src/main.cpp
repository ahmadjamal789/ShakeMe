// main.cpp - shake-to-mix display: Brownian dust that settles at rest and
// scatters like a gas when the device is shaken.
//
// Frame loop (this is what keeps it smooth):
//
//   1. draw the current particle state into the framebuffer     (CPU, ~2 ms)
//   2. start a DMA transfer of the framebuffer to the display   (instant)
//   3. read the IMU and run physics for the next frame           (CPU, while DMA runs)
//   4. buttons, battery, idle dimming, diagnostics
//   5. wait for the DMA transfer to finish
//
// Steps 2-5 overlap, so the frame time is roughly max(transfer, physics) instead
// of their sum.
#include <Arduino.h>
#include <stdio.h>

#include "board.h"
#include "config.h"
#include "imu_mpu6050.h"
#include "motion.h"
#include "particles.h"
#include "render.h"
#include "st7789_dma.h"

namespace {

enum class View : uint8_t { Beaker, Graph };

uint16_t g_fb[SCREEN_W * SCREEN_H] __attribute__((aligned(4)));
dust::Engine g_engine;
Motion g_motion;
Mpu6050 g_imu;
St7789 g_tft;
Backlight g_backlight;
Battery g_battery;
Button g_btnMode, g_btnGravity;
gfx::GraphHistory g_graph;

View g_view = View::Beaker;
uint32_t g_lastPhysUs = 0, g_accUs = 0;
uint32_t g_lastActivityMs = 0, g_badgeUntilMs = 0, g_imuRetryMs = 0, g_diagMs = 0;
uint32_t g_lastFrameUs = 0;
float g_fps = 0;
bool g_dimmed = false;

void wake(uint32_t nowMs) {
  g_lastActivityMs = nowMs;
  if (g_dimmed) {
    g_backlight.set(BRIGHT_FULL);
    g_dimmed = false;
  }
}

// Reads the IMU and advances the simulation by however much real time passed.
void runPhysics() {
  uint32_t nowUs = micros();
  g_accUs += nowUs - g_lastPhysUs;
  g_lastPhysUs = nowUs;
  const uint32_t maxAcc = (uint32_t)STEP_US * 8;  // never try to catch up more than 8 steps
  if (g_accUs > maxAcc) g_accUs = maxAcc;

  while (g_accUs >= (uint32_t)STEP_US) {
    float a[3] = {0, 0, 0};
    bool ok = g_imu.readAccelG(a);
    g_engine.step(g_motion.update(a, ok));
    g_accUs -= (uint32_t)STEP_US;
  }
  g_graph.push(g_motion.linearG(), g_motion.agitation());
}

void handleInput(uint32_t nowMs) {
  if (g_btnMode.pressed(nowMs)) {
    g_view = (g_view == View::Beaker) ? View::Graph : View::Beaker;
    wake(nowMs);
  }
  if (g_btnGravity.pressed(nowMs)) {
    g_motion.setTiltMode(!g_motion.tiltMode());
    g_badgeUntilMs = nowMs + 1500;
    wake(nowMs);
  }
  if (g_motion.agitation() > 0.05f) wake(nowMs);
  if (!g_dimmed && nowMs - g_lastActivityMs > IDLE_DIM_MS) {
    g_backlight.set(BRIGHT_DIM);
    g_dimmed = true;
  }
  // If the sensor dropped off the bus, try to bring it back every 2 s.
  if (!g_imu.ok() && nowMs - g_imuRetryMs > 2000) {
    g_imuRetryMs = nowMs;
    g_imu.init();
  }
}

void drawOverlays(const gfx::Canvas& cv, uint32_t nowMs) {
#if SHOW_BATTERY
  gfx::drawBattery(cv, g_battery.percent(), g_battery.usbPowered());
#endif
  if (!g_imu.ok()) gfx::drawWarning(cv);
  if (nowMs < g_badgeUntilMs) gfx::drawBadge(cv, g_motion.tiltMode() ? "TILT" : "FIXED");
#if SHOW_FPS
  char buf[16];
  snprintf(buf, sizeof buf, "FPS %d", (int)(g_fps + 0.5f));
  gfx::drawText(cv, 8, SCREEN_H - 14, buf, 2, gfx::rgb(255, 255, 255));
#endif
}

void printDiagnostics(uint32_t nowMs) {
#if DIAG_SERIAL
  if (nowMs - g_diagMs < 1000) return;
  g_diagMs = nowMs;
  const float* s = g_motion.screen();
  Serial.printf("fps=%.1f agit=%.2f shake=%.2fg  s=(%.2f, %.2f, %.2f)  bat=%.2fV %d%%%s  imu=%s errors=%lu\n",
                g_fps, g_motion.agitation(), g_motion.linearG(), s[0], s[1], s[2], g_battery.volts(),
                g_battery.percent(), g_battery.usbPowered() ? " (USB)" : "", g_imu.ok() ? "ok" : "FAIL",
                (unsigned long)g_imu.errorCount());
#else
  (void)nowMs;
#endif
}

}  // namespace

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);
  Serial.println("shake-to-mix firmware starting");

  g_backlight.init();  // starts off, so you never see random GRAM noise
  g_tft.init();
  g_tft.fillColor(0);
  g_backlight.set(BRIGHT_FULL);

#if BOOT_SELFTEST
  g_tft.fillColor(gfx::rgb(255, 0, 0));   delay(250);
  g_tft.fillColor(gfx::rgb(0, 255, 0));   delay(250);
  g_tft.fillColor(gfx::rgb(0, 0, 255));   delay(250);
  g_tft.fillColor(0);
#endif

  bool imuOk = g_imu.init();
  Serial.printf("MPU6050: %s (WHO_AM_I = 0x%02X)\n", imuOk ? "ok" : "NOT FOUND - check wiring/address", g_imu.whoAmI());
  Serial.println("Orientation check: hold the device upright, expect s=(~0, ~-1.00, ~0).");

  g_motion.init();
  g_engine.init(micros() * 2654435761u ^ ((uint32_t)analogRead(A0) << 16));
  gfx::init(g_engine);

  g_btnMode.init(BTN_PIN_MODE);
  g_btnGravity.init(BTN_PIN_GRAVITY);
  g_battery.init();
  g_battery.update(millis());

  g_lastPhysUs = micros();
  g_lastFrameUs = g_lastPhysUs;
  wake(millis());
}

void loop() {
  const uint32_t nowMs = millis();
  const gfx::Canvas cv{g_fb};

  // 1. draw
  if (g_view == View::Beaker)
    gfx::drawBeaker(cv, g_engine);
  else
    gfx::drawGraph(cv, g_graph);
  drawOverlays(cv, nowMs);

  // 2. start DMA transfer to the display
  g_tft.startFrame(g_fb);

  // 3. physics for the next frame, while the transfer runs
  runPhysics();

  // 4. everything else
  handleInput(nowMs);
  g_battery.update(nowMs);
  printDiagnostics(nowMs);

  // 5. wait for the transfer, then update the frame-rate estimate
  g_tft.waitFrame();
  uint32_t nowUs = micros();
  float inst = 1e6f / (float)(nowUs - g_lastFrameUs + 1);
  g_fps = g_fps == 0 ? inst : 0.9f * g_fps + 0.1f * inst;
  g_lastFrameUs = nowUs;
}
