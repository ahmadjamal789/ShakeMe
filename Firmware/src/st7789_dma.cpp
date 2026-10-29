#include "st7789_dma.h"

#include <Arduino.h>
#include "config.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"

namespace {
constexpr size_t FRAME_BYTES = (size_t)SCREEN_W * SCREEN_H * 2;

inline void csLow() { gpio_put(TFT_PIN_CS, 0); }
inline void csHigh() { gpio_put(TFT_PIN_CS, 1); }
inline void dcCommand() { gpio_put(TFT_PIN_DC, 0); }
inline void dcData() { gpio_put(TFT_PIN_DC, 1); }

inline void waitSpiIdle() {
  while (spi_get_hw(TFT_SPI_PORT)->sr & SPI_SSPSR_BSY_BITS) tight_loop_contents();
}
}  // namespace

void St7789::command(uint8_t cmd) { command(cmd, nullptr, 0); }

void St7789::command(uint8_t cmd, const uint8_t* data, uint8_t len) {
  csLow();
  dcCommand();
  spi_write_blocking(TFT_SPI_PORT, &cmd, 1);
  if (len) {
    dcData();
    spi_write_blocking(TFT_SPI_PORT, data, len);
  }
  csHigh();
}

void St7789::init() {
  gpio_init(TFT_PIN_CS);  gpio_set_dir(TFT_PIN_CS, GPIO_OUT);  csHigh();
  gpio_init(TFT_PIN_DC);  gpio_set_dir(TFT_PIN_DC, GPIO_OUT);  dcData();
#if TFT_PIN_RST >= 0
  gpio_init(TFT_PIN_RST); gpio_set_dir(TFT_PIN_RST, GPIO_OUT);
#endif

  spi_init(TFT_SPI_PORT, TFT_SPI_HZ);
  spi_set_format(TFT_SPI_PORT, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);  // SPI mode 3
  gpio_set_function(TFT_PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(TFT_PIN_MOSI, GPIO_FUNC_SPI);

#if TFT_PIN_RST >= 0
  gpio_put(TFT_PIN_RST, 1); delay(10);
  gpio_put(TFT_PIN_RST, 0); delay(20);
  gpio_put(TFT_PIN_RST, 1); delay(120);
#else
  command(0x01); delay(150);  // software reset
#endif

  command(0x11); delay(120);                                       // sleep out
  command(0x13);                                                   // normal display mode
  { const uint8_t d[] = {TFT_MADCTL}; command(0x36, d, 1); }       // orientation / colour order
  { const uint8_t d[] = {0x55}; command(0x3A, d, 1); }             // 16 bit colour
  { const uint8_t d[] = {0x0C, 0x0C, 0x00, 0x33, 0x33}; command(0xB2, d, 5); }  // porch
  { const uint8_t d[] = {0x35}; command(0xB7, d, 1); }             // gate control
  { const uint8_t d[] = {0x28}; command(0xBB, d, 1); }             // VCOM
  { const uint8_t d[] = {0x0C}; command(0xC0, d, 1); }             // LCM control
  { const uint8_t d[] = {0x01, 0xFF}; command(0xC2, d, 2); }       // VDV/VRH enable
  { const uint8_t d[] = {0x10}; command(0xC3, d, 1); }             // VRH
  { const uint8_t d[] = {0x20}; command(0xC4, d, 1); }             // VDV
  { const uint8_t d[] = {0x0F}; command(0xC6, d, 1); }             // frame rate
  { const uint8_t d[] = {0xA4, 0xA1}; command(0xD0, d, 2); }       // power control
  { const uint8_t d[] = {0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x32, 0x44, 0x42, 0x06, 0x0E, 0x12, 0x14, 0x17};
    command(0xE0, d, 14); }                                        // positive gamma
  { const uint8_t d[] = {0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x31, 0x54, 0x47, 0x0E, 0x1C, 0x17, 0x1B, 0x1E};
    command(0xE1, d, 14); }                                        // negative gamma
  command(TFT_INVERT ? 0x21 : 0x20);                               // inversion on/off

  // Full-screen drawing window (portrait 240 x 320).
  { const uint8_t d[] = {0x00, 0x00, (uint8_t)((SCREEN_W - 1) >> 8), (uint8_t)((SCREEN_W - 1) & 0xFF)};
    command(0x2A, d, 4); }
  { const uint8_t d[] = {0x00, 0x00, (uint8_t)((SCREEN_H - 1) >> 8), (uint8_t)((SCREEN_H - 1) & 0xFF)};
    command(0x2B, d, 4); }
  command(0x29); delay(20);                                        // display on

  dma_ = dma_claim_unused_channel(true);
}

void St7789::startFrame(const uint16_t* framebuffer) {
  csLow();
  dcCommand();
  uint8_t ramwr = 0x2C;
  spi_write_blocking(TFT_SPI_PORT, &ramwr, 1);
  dcData();

  dma_channel_config cfg = dma_channel_get_default_config(dma_);
  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
  channel_config_set_dreq(&cfg, spi_get_dreq(TFT_SPI_PORT, true));
  channel_config_set_read_increment(&cfg, true);
  channel_config_set_write_increment(&cfg, false);
  dma_channel_configure(dma_, &cfg, &spi_get_hw(TFT_SPI_PORT)->dr, framebuffer, FRAME_BYTES, true);
  inFrame_ = true;
}

void St7789::waitFrame() {
  if (!inFrame_) return;
  dma_channel_wait_for_finish_blocking(dma_);
  waitSpiIdle();
  csHigh();
  inFrame_ = false;
}

void St7789::fillColor(uint16_t wireColor) {
  static uint16_t line[SCREEN_W];
  for (int i = 0; i < SCREEN_W; ++i) line[i] = wireColor;
  csLow();
  dcCommand();
  uint8_t ramwr = 0x2C;
  spi_write_blocking(TFT_SPI_PORT, &ramwr, 1);
  dcData();
  for (int y = 0; y < SCREEN_H; ++y) spi_write_blocking(TFT_SPI_PORT, (const uint8_t*)line, sizeof(line));
  waitSpiIdle();
  csHigh();
}
