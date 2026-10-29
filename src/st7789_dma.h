// st7789_dma.h - ST7789 240x320 driver that pushes a whole RGB565 framebuffer by DMA.
//
// startFrame() returns immediately; the CPU is free to run physics while the
// DMA engine streams ~150 KB to the display. Call waitFrame() before touching
// the framebuffer again.
#pragma once
#include <stdint.h>

class St7789 {
 public:
  void init();
  void startFrame(const uint16_t* framebuffer);  // framebuffer bytes already in wire order
  void waitFrame();
  void fillColor(uint16_t wireColor);            // blocking solid fill (self-test)

 private:
  void command(uint8_t cmd);
  void command(uint8_t cmd, const uint8_t* data, uint8_t len);
  int dma_ = -1;
  bool inFrame_ = false;
};
