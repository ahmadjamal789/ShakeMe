// render.h - draws everything into a 240x320 RGB565 framebuffer in RAM.
//
// Pure C++ (no hardware), so the exact same drawing code runs on the Pico and on
// a PC for screenshots. Colours are stored byte-swapped, which is the order the
// ST7789 wants on the SPI wire, so the framebuffer can be sent as-is by DMA.
#pragma once
#include <stdint.h>
#include "config.h"
#include "particles.h"

namespace gfx {

// RGB888 -> RGB565 with the two bytes swapped for the SPI wire.
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)((((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)) >> 8) |
         (uint16_t)((((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)) << 8);
}

struct Canvas {
  uint16_t* px;
  void pixel(int x, int y, uint16_t c) const;
  void fillRect(int x, int y, int w, int h, uint16_t c) const;
  void line(int x0, int y0, int x1, int y1, uint16_t c) const;
};

// Rolling history for the graph view (one column per sample).
struct GraphHistory {
  uint8_t lin[SCREEN_W] = {0};    // shaking strength, 0..4 g mapped to 0..255
  uint8_t agit[SCREEN_W] = {0};   // agitation, 0..1 mapped to 0..255
  int head = 0;
  void push(float lin_g, float agitation);
};

void init(const dust::Engine& e);            // call once after engine init
void drawBeaker(const Canvas& c, const dust::Engine& e);
void drawGraph(const Canvas& c, const GraphHistory& h);
void drawText(const Canvas& c, int x, int y, const char* s, int scale, uint16_t color);
void drawBadge(const Canvas& c, const char* text);               // boxed label, top-left
void drawBattery(const Canvas& c, int percent, bool usbPowered); // top-right icon
void drawWarning(const Canvas& c);                               // red square (IMU problem)

}  // namespace gfx
