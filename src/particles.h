// particles.h - fixed-point 2D particle engine ("Brownian dust in a box").
//
// Pure C++: no Arduino, no pico-sdk. That is deliberate so the same code runs
// on the RP2040 and on a PC for testing (see test/host).
//
// The RP2040 has no floating point unit, so all per-particle work is done in
// integers: positions and velocities are Q12 fixed point (1 px = 4096 units),
// velocities are in pixels per physics step.
#pragma once
#include <stdint.h>
#include "config.h"

namespace dust {

constexpr int Q = 12;                  // fractional bits
constexpr int32_t ONE = 1 << Q;        // 1 pixel
constexpr int CELL_SHIFT = 3;          // collision grid cell = 8 px
constexpr int GRID_W = (SCREEN_W + 7) >> CELL_SHIFT;
constexpr int GRID_H = (SCREEN_H + 7) >> CELL_SHIFT;
constexpr int GRID_CELLS = GRID_W * GRID_H;

// What the world does to the particles during one physics step.
struct StepInput {
  int32_t ax = 0;    // acceleration, Q12 px/step^2, screen coords (+y is down)
  int32_t ay = 0;
  int32_t kick = 0;  // random velocity kick amplitude, Q12 px/step (0 = none)
};

class Engine {
 public:
  void init(uint32_t seed);
  void step(const StepInput& in);

  int count() const { return PARTICLE_COUNT; }
  float radiusPx() const { return (float)rQ12_ / (float)ONE; }
  int32_t radiusQ12() const { return rQ12_; }
  const int32_t* x() const { return x_; }
  const int32_t* y() const { return y_; }
  const int32_t* vx() const { return vx_; }
  const int32_t* vy() const { return vy_; }

  // statistics (used by the host tests)
  uint32_t pairChecks = 0;
  uint32_t pairContacts = 0;

 private:
  void integrate(const StepInput& in);
  void walls();
  void buildGrid();
  void collide();
  void scanCell(int i, int cell);
  void resolve(int a, int b);
  void sleepDamp();
  uint32_t rnd();

  int32_t x_[PARTICLE_COUNT];
  int32_t y_[PARTICLE_COUNT];
  int32_t vx_[PARTICLE_COUNT];
  int32_t vy_[PARTICLE_COUNT];

  uint16_t start_[GRID_CELLS + 1];
  uint16_t fill_[GRID_CELLS];
  uint16_t cellOf_[PARTICLE_COUNT];
  uint16_t order_[PARTICLE_COUNT];
  uint8_t touched_[PARTICLE_COUNT];   // 1 if in contact during this step

  int32_t rQ12_ = 0;     // particle radius, Q12
  int32_t twoRQ8_ = 0;   // particle diameter, Q8
  int32_t d2Q16_ = 0;    // diameter squared, Q16
  uint32_t rng_ = 1;
};

}  // namespace dust
