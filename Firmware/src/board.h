// board.h - small board-level helpers: backlight PWM, debounced buttons, battery gauge.
#pragma once
#include <stdint.h>

class Backlight {
 public:
  void init();
  void set(uint8_t level);  // 0..255
};

// Active-low push button (wired to GND, internal pull-up).
class Button {
 public:
  void init(int pin);
  bool pressed(uint32_t nowMs);  // true exactly once per press, debounced

 private:
  int pin_ = -1;
  bool raw_ = false, stable_ = false;
  uint32_t changedMs_ = 0;
};

class Battery {
 public:
  void init();
  void update(uint32_t nowMs);          // samples every ~2 s
  float volts() const { return volts_; }  // estimated cell voltage
  int percent() const;                    // 0..100
  bool usbPowered() const { return usb_; }

 private:
  float volts_ = 4.0f;
  bool usb_ = false;
  uint32_t lastMs_ = 0;
  bool primed_ = false;
};
