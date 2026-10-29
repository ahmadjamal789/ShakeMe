#pragma once
#include <stddef.h>
#include <stdint.h>
#include "hardware/gpio.h"
struct spi_inst {};
typedef struct spi_inst spi_inst_t;
extern spi_inst_t g_spi0_mock;
#define spi0 (&g_spi0_mock)
typedef enum { SPI_CPOL_0 = 0, SPI_CPOL_1 = 1 } spi_cpol_t;
typedef enum { SPI_CPHA_0 = 0, SPI_CPHA_1 = 1 } spi_cpha_t;
typedef enum { SPI_LSB_FIRST = 0, SPI_MSB_FIRST = 1 } spi_order_t;
struct spi_hw_t { volatile uint32_t dr; volatile uint32_t sr; };
#define SPI_SSPSR_BSY_BITS 0x10u
inline uint32_t spi_init(spi_inst_t*, uint32_t) { return 0; }
inline void spi_set_format(spi_inst_t*, uint32_t, spi_cpol_t, spi_cpha_t, spi_order_t) {}
inline int spi_write_blocking(spi_inst_t*, const uint8_t*, size_t n) { return (int)n; }
inline unsigned spi_get_dreq(spi_inst_t*, bool) { return 0; }
inline spi_hw_t* spi_get_hw(spi_inst_t*) { static spi_hw_t hw; return &hw; }
