#pragma once
#include <stdint.h>
enum gpio_function { GPIO_FUNC_SPI = 1, GPIO_FUNC_I2C = 3 };
enum { GPIO_OUT = 1, GPIO_IN = 0 };
inline void gpio_init(unsigned) {}
inline void gpio_set_dir(unsigned, bool) {}
inline void gpio_put(unsigned, bool) {}
inline void gpio_set_function(unsigned, gpio_function) {}
inline void gpio_pull_up(unsigned) {}
inline void tight_loop_contents() {}
