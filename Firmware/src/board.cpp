#include "board.h"

#include <Arduino.h>
#include "config.h"

// ---- backlight --------------------------------------------------------------
void Backlight::init() {
  pinMode(TFT_PIN_BL, OUTPUT);
  analogWriteFreq(BL_PWM_HZ);
  analogWriteRange(255);
  set(0);
}

void Backlight::set(uint8_t level) {
  analogWrite(TFT_PIN_BL, BL_ACTIVE_HIGH ? level : 255 - level);
}

// ---- buttons ----------------------------------------------------------------
void Button::init(int pin) {
  pin_ = pin;
  pinMode(pin_, INPUT_PULLUP);
  raw_ = stable_ = (digitalRead(pin_) == LOW);
  changedMs_ = millis();
}

bool Button::pressed(uint32_t nowMs) {
  bool now = (digitalRead(pin_) == LOW);
  if (now != raw_) {
    raw_ = now;
    changedMs_ = nowMs;
  }
  if (nowMs - changedMs_ >= 30 && raw_ != stable_) {
    stable_ = raw_;
    return stable_;  // report the press, not the release
  }
  return false;
}

// ---- battery ----------------------------------------------------------------
// Pico GPIO29 (A3) measures VSYS / 3. The battery feeds VSYS through a Schottky
// diode, so cell voltage ~ VSYS + diode drop. On USB power VSYS is ~4.7 V.
void Battery::init() {
  analogReadResolution(12);
  primed_ = false;
  lastMs_ = 0;
}

void Battery::update(uint32_t nowMs) {
  if (primed_ && nowMs - lastMs_ < 2000) return;
  lastMs_ = nowMs;
  uint32_t sum = 0;
  for (int i = 0; i < 8; ++i) sum += analogRead(A3);
  float vsys = ((float)sum / 8.0f) * 3.3f / 4095.0f * 3.0f;
  usb_ = vsys > 4.45f;
  float v = vsys + BAT_DIODE_DROP_V;
  volts_ = primed_ ? (0.7f * volts_ + 0.3f * v) : v;
  primed_ = true;
}

int Battery::percent() const {
  static const struct { float v; int pct; } curve[] = {
      {4.20f, 100}, {4.10f, 90}, {4.00f, 78}, {3.90f, 62}, {3.80f, 45},
      {3.70f, 28},  {3.60f, 12}, {3.50f, 5},  {3.30f, 0}};
  const int n = (int)(sizeof(curve) / sizeof(curve[0]));
  if (volts_ >= curve[0].v) return 100;
  for (int i = 1; i < n; ++i) {
    if (volts_ >= curve[i].v) {
      float t = (volts_ - curve[i].v) / (curve[i - 1].v - curve[i].v);
      return curve[i].pct + (int)(t * (float)(curve[i - 1].pct - curve[i].pct));
    }
  }
  return 0;
}
