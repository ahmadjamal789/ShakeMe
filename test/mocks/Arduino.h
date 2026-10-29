// Minimal stand-in for Arduino.h so the firmware can be syntax-checked on a PC.
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "hardware/gpio.h"

#define LOW 0
#define HIGH 1
#define INPUT_PULLUP 2
#define OUTPUT 1
#define A0 26
#define A3 29

inline uint32_t millis() { return 0; }
inline uint32_t micros() { return 0; }
inline void delay(uint32_t) {}
inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void analogWrite(int, int) {}
inline void analogWriteFreq(uint32_t) {}
inline void analogWriteRange(uint32_t) {}
inline void analogReadResolution(int) {}
inline int analogRead(int) { return 2048; }

struct SerialMock {
  void begin(unsigned long) {}
  void println(const char*) {}
  explicit operator bool() const { return true; }
  int printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) { (void)fmt; return 0; }
};
static SerialMock Serial;
