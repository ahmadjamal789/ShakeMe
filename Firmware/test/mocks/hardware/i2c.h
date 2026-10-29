#pragma once
#include <stddef.h>
#include <stdint.h>
struct i2c_inst {};
typedef struct i2c_inst i2c_inst_t;
extern i2c_inst_t g_i2c1_mock;
#define i2c1 (&g_i2c1_mock)
inline uint32_t i2c_init(i2c_inst_t*, uint32_t) { return 0; }
inline int i2c_write_timeout_us(i2c_inst_t*, uint8_t, const uint8_t*, size_t n, bool, uint32_t) { return (int)n; }
inline int i2c_read_timeout_us(i2c_inst_t*, uint8_t, uint8_t*, size_t n, bool, uint32_t) { return (int)n; }
