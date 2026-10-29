// config.h - every setting you are likely to change lives here.
//
// 1. WIRING        must match your schematic
// 2. DISPLAY       ST7789 240x320 settings
// 3. IMU           MPU6050 and axis mapping
// 4. SIMULATION    how the particles look and behave
// 5. UI / POWER    brightness, idle dimming, battery
#pragma once
#include <stdint.h>

// ============================================================================
// 1. WIRING  (Pico GPIO numbers) - EDIT TO MATCH YOUR SCHEMATIC
// ============================================================================
#define TFT_SPI_PORT spi0
#define TFT_PIN_SCK 2    // SPI0 SCK  (valid: 2, 6, 18, 22)
#define TFT_PIN_MOSI 3   // SPI0 TX   (valid: 3, 7, 19)
#define TFT_PIN_CS 1
#define TFT_PIN_DC 0
#define TFT_PIN_RST 4    // set to -1 if the reset pin is not wired
#define TFT_PIN_BL 5     // backlight (see BL_ACTIVE_HIGH below)

#define IMU_I2C_PORT i2c1
#define IMU_PIN_SDA 6    // I2C1 SDA (valid: 2, 6, 10, 14, 18, 26)
#define IMU_PIN_SCL 7    // I2C1 SCL (valid: 3, 7, 11, 15, 19, 27)
#define IMU_I2C_ADDR 0x68  // MPU6050 AD0 low = 0x68, AD0 high = 0x69

#define BTN_PIN_MODE 8     // cycles Beaker <-> Graph (button to GND)
#define BTN_PIN_GRAVITY 9  // toggles fixed gravity <-> tilt gravity (button to GND)

// ============================================================================
// 2. DISPLAY
// ============================================================================
#define SCREEN_W 240
#define SCREEN_H 320
#define TFT_SPI_HZ 40000000  // try 62500000 together with f_cpu = 250 MHz
#define TFT_MADCTL 0x00      // 0x00 portrait, 0xC0 upside down, 0x08 = swap red/blue
#define TFT_INVERT 1         // most IPS ST7789 modules need inversion ON
#define BL_ACTIVE_HIGH 1     // 1: HIGH turns the backlight on. 0: LOW turns it on.
#define BL_PWM_HZ 20000

// ============================================================================
// 3. IMU (MPU6050) AND ORIENTATION
// ============================================================================
// Map IMU axes onto screen axes. Screen +x is right, screen +y is DOWN.
// Axis numbers: 0 = IMU X, 1 = IMU Y, 2 = IMU Z.
//
// Calibration: hold the device upright (screen facing you, top edge up) and
// read the serial monitor. You want  sx ~ 0,  sy ~ -1.00,  sz ~ 0.
// If the -1 shows up on the wrong axis or with the wrong sign, change below.
#define MAP_SX_AXIS 0
#define MAP_SX_SIGN (+1)
#define MAP_SY_AXIS 1
#define MAP_SY_SIGN (-1)
#define MAP_SZ_AXIS 2
#define MAP_SZ_SIGN (+1)

// ============================================================================
// 4. SIMULATION  (the "feel" knobs)
// ============================================================================
#define PARTICLE_COUNT 400        // more = denser pile, slower physics (max ~600)
#define SETTLED_FRACTION 0.15f    // pile height at rest, as a fraction of screen height
#define PACKING 0.86f             // calibrated with test/host so the pile ends up SETTLED_FRACTION tall
#define PHYS_HZ 120               // physics steps per second

#define GRAVITY_PX_S2 220.0f      // downward pull, pixels / s^2
#define DRAG_PER_S 1.2f           // solvent drag. Higher = slower, more syrupy
#define SHAKE_PX_S2_PER_G 900.0f  // how hard 1 g of shaking throws the particles
#define KICK_PX_PER_STEP 0.6f     // Brownian jitter at full agitation (px/step)
#define E_WALL 0.80f              // wall bounciness   (0 = dead, 1 = perfect)
#define E_PART 0.85f              // particle-particle bounciness
#define FRICTION 0.10f            // tangential damping in contacts (pile stability)
#define FLOOR_FRICTION 0.08f      // sliding friction on the floor
#define V_MAX_PX_S 900.0f         // speed limit (prevents tunnelling)
#define REST_SPEED_PX_S 15.0f     // below this impact speed, collisions are inelastic
#define SLEEP_SPEED_PX_S 30.0f    // slow + touching + not stirred = damped to rest
#define COLLISION_ITERS 3

// Agitation = a smoothed "how recently and how hard was I shaken" value, 0..1.
#define AGIT_DEADBAND_G 0.15f     // shaking below this is ignored (sensor noise)
#define AGIT_FULL_G 2.5f          // shaking at this strength = full agitation
#define AGIT_TAU_S 1.2f           // agitation fades with this time constant
#define GRAV_LP_TAU_S 0.6f        // gravity estimate filter (separates shake from gravity)
#define BOOT_AGITATION 0.5f       // starts stirred up, then settles (set 0 to start settled)

// ============================================================================
// 5. UI / POWER
// ============================================================================
#define BRIGHT_FULL 255
#define BRIGHT_DIM 30
#define IDLE_DIM_MS 30000         // dim the backlight after this long without motion
#define SHOW_BATTERY 1
#define SHOW_FPS 0
#define BOOT_SELFTEST 1           // red/green/blue flash at power-up
#define DIAG_SERIAL 1             // print a status line once per second
#define BAT_DIODE_DROP_V 0.30f    // Schottky drop between battery and VSYS

// ---- derived (do not edit) -------------------------------------------------
#define STEP_US (1000000 / PHYS_HZ)
