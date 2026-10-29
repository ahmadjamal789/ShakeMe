#include "particles.h"

#include <math.h>
#include <string.h>

namespace dust {

// ---- constants derived from config.h (all compile-time) --------------------
constexpr int32_t E_WALL_Q8 = (int32_t)(E_WALL * 256.0f);
constexpr int32_t E_PART_Q8 = (int32_t)(E_PART * 256.0f);
constexpr int32_t FRIC_Q8 = (int32_t)(FRICTION * 256.0f);
constexpr int32_t FLOOR_FRIC_Q8 = (int32_t)(FLOOR_FRICTION * 256.0f);
constexpr int32_t V_MAX_Q12 = (int32_t)(V_MAX_PX_S * (float)ONE / (float)PHYS_HZ);
constexpr int32_t REST_Q12 = (int32_t)(REST_SPEED_PX_S * (float)ONE / (float)PHYS_HZ);
constexpr int32_t SLEEP_Q12 = (int32_t)(SLEEP_SPEED_PX_S * (float)ONE / (float)PHYS_HZ);
constexpr int32_t DRAG_DIV = (int32_t)((float)PHYS_HZ / DRAG_PER_S + 0.5f);

static_assert(PARTICLE_COUNT > 0 && PARTICLE_COUNT < 4000, "PARTICLE_COUNT out of range");
static_assert(DRAG_DIV >= 2, "DRAG_PER_S too high for PHYS_HZ");

// ---- small integer helpers ---------------------------------------------------
static inline int32_t absi(int32_t v) { return v < 0 ? -v : v; }

static inline int32_t clampi(int32_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Integer square root (Newton, start above the root). Good for n < 2^30.
static inline uint32_t isqrt32(uint32_t n) {
  if (n < 2) return n;
  uint32_t x = 1u << ((33 - __builtin_clz(n)) >> 1);
  x = (x + n / x) >> 1;
  x = (x + n / x) >> 1;
  x = (x + n / x) >> 1;
  return x;
}

// Solvent drag: removes 1/DRAG_DIV of the speed, rounded AWAY from zero so tiny
// velocities always decay to exactly zero (and never drift in one direction).
static inline int32_t dragStep(int32_t v) {
  return v >= 0 ? (v + DRAG_DIV - 1) / DRAG_DIV : (v - DRAG_DIV + 1) / DRAG_DIV;
}

uint32_t Engine::rnd() {  // xorshift32
  uint32_t v = rng_;
  v ^= v << 13;
  v ^= v >> 17;
  v ^= v << 5;
  rng_ = v;
  return v;
}

void Engine::init(uint32_t seed) {
  rng_ = seed ? seed : 0x9E3779B9u;
  pairChecks = pairContacts = 0;

  // Particle radius chosen so the settled pile is SETTLED_FRACTION of the screen.
  float r = sqrtf(PACKING * (float)SCREEN_W * (SETTLED_FRACTION * (float)SCREEN_H) /
                  ((float)PARTICLE_COUNT * 3.14159265f));
  if (r > 3.9f) r = 3.9f;  // grid cell is 8 px, so diameter must stay below it
  if (r < 1.2f) r = 1.2f;
  rQ12_ = (int32_t)(r * (float)ONE);
  twoRQ8_ = (2 * rQ12_) / 16;
  d2Q16_ = twoRQ8_ * twoRQ8_;

  // Start as a jittered lattice filling the upper part of the box.
  const float topFrac = 0.62f;
  float spacing = sqrtf((float)SCREEN_W * ((float)SCREEN_H * topFrac) / (float)PARTICLE_COUNT);
  int cols = (int)((float)SCREEN_W / spacing);
  if (cols < 1) cols = 1;
  int rows = (PARTICLE_COUNT + cols - 1) / cols;
  float cellW = (float)SCREEN_W / (float)cols;
  float cellH = ((float)SCREEN_H * topFrac) / (float)rows;
  for (int i = 0; i < PARTICLE_COUNT; ++i) {
    int col = i % cols, row = i / cols;
    float jx = ((float)(rnd() & 0xFF) / 255.0f - 0.5f) * cellW * 0.5f;
    float jy = ((float)(rnd() & 0xFF) / 255.0f - 0.5f) * cellH * 0.5f;
    float px = (col + 0.5f) * cellW + jx;
    float py = r + (row + 0.5f) * cellH + jy;
    x_[i] = clampi((int32_t)(px * (float)ONE), rQ12_, SCREEN_W * ONE - rQ12_);
    y_[i] = clampi((int32_t)(py * (float)ONE), rQ12_, SCREEN_H * ONE - rQ12_);
    vx_[i] = vy_[i] = 0;
  }
}

void Engine::step(const StepInput& in) {
  memset(touched_, 0, sizeof(touched_));
  integrate(in);
  walls();
  buildGrid();
  for (int it = 0; it < COLLISION_ITERS; ++it) {
    collide();
    walls();
  }
  // Slow particles that are touching something are damped hard, so a resting
  // pile goes completely still. Skipped while the system is being stirred.
  if (in.kick == 0) sleepDamp();
}

void Engine::sleepDamp() {
  for (int i = 0; i < PARTICLE_COUNT; ++i) {
    if (!touched_[i]) continue;
    if (absi(vx_[i]) + absi(vy_[i]) < SLEEP_Q12) {
      vx_[i] -= vx_[i] / 2;
      vy_[i] -= vy_[i] / 2;
    }
  }
}

void Engine::integrate(const StepInput& in) {
  for (int i = 0; i < PARTICLE_COUNT; ++i) {
    int32_t vx = vx_[i] + in.ax;
    int32_t vy = vy_[i] + in.ay;
    if (in.kick > 0) {
      int32_t r1 = (int32_t)(rnd() & 0xFFFF) - 32768;
      int32_t r2 = (int32_t)(rnd() & 0xFFFF) - 32768;
      vx += (r1 * in.kick) / 32768;  // '/' truncates toward zero: no bias
      vy += (r2 * in.kick) / 32768;
    }
    vx -= dragStep(vx);
    vy -= dragStep(vy);
    vx = clampi(vx, -V_MAX_Q12, V_MAX_Q12);
    vy = clampi(vy, -V_MAX_Q12, V_MAX_Q12);
    vx_[i] = vx;
    vy_[i] = vy;
    x_[i] += vx;
    y_[i] += vy;
  }
}

void Engine::walls() {
  const int32_t minX = rQ12_, maxX = SCREEN_W * ONE - rQ12_;
  const int32_t minY = rQ12_, maxY = SCREEN_H * ONE - rQ12_;
  for (int i = 0; i < PARTICLE_COUNT; ++i) {
    int32_t px = x_[i], py = y_[i], vx = vx_[i], vy = vy_[i];

    if (px < minX) {
      px = minX;
      if (vx < 0) vx = (-vx < REST_Q12) ? 0 : ((-vx) * E_WALL_Q8) / 256;
    } else if (px > maxX) {
      px = maxX;
      if (vx > 0) vx = (vx < REST_Q12) ? 0 : -((vx * E_WALL_Q8) / 256);
    }

    if (py < minY) {
      py = minY;
      if (vy < 0) vy = (-vy < REST_Q12) ? 0 : ((-vy) * E_WALL_Q8) / 256;
    } else if (py > maxY) {
      py = maxY;
      if (vy > 0) vy = (vy < REST_Q12) ? 0 : -((vy * E_WALL_Q8) / 256);
      vx -= (vx * FLOOR_FRIC_Q8) / 256;  // sliding friction on the floor
      touched_[i] = 1;
    }

    x_[i] = px;
    y_[i] = py;
    vx_[i] = vx;
    vy_[i] = vy;
  }
}

void Engine::buildGrid() {
  memset(start_, 0, sizeof(start_));
  for (int i = 0; i < PARTICLE_COUNT; ++i) {
    int32_t cx = clampi(x_[i] >> (Q + CELL_SHIFT), 0, GRID_W - 1);
    int32_t cy = clampi(y_[i] >> (Q + CELL_SHIFT), 0, GRID_H - 1);
    uint16_t c = (uint16_t)(cy * GRID_W + cx);
    cellOf_[i] = c;
    ++start_[c + 1];
  }
  for (int c = 0; c < GRID_CELLS; ++c) start_[c + 1] += start_[c];
  memcpy(fill_, start_, sizeof(fill_));
  for (int i = 0; i < PARTICLE_COUNT; ++i) order_[fill_[cellOf_[i]]++] = (uint16_t)i;
}

void Engine::scanCell(int i, int cell) {
  for (int k = start_[cell]; k < start_[cell + 1]; ++k) resolve(i, order_[k]);
}

void Engine::collide() {
  for (int cy = 0; cy < GRID_H; ++cy) {
    for (int cx = 0; cx < GRID_W; ++cx) {
      const int c = cy * GRID_W + cx;
      const int s = start_[c], e = start_[c + 1];
      for (int a = s; a < e; ++a) {
        const int i = order_[a];
        for (int b = a + 1; b < e; ++b) resolve(i, order_[b]);  // same cell
        // forward neighbours only, so every pair is visited exactly once
        if (cx + 1 < GRID_W) scanCell(i, c + 1);
        if (cy + 1 < GRID_H) {
          if (cx > 0) scanCell(i, c + GRID_W - 1);
          scanCell(i, c + GRID_W);
          if (cx + 1 < GRID_W) scanCell(i, c + GRID_W + 1);
        }
      }
    }
  }
}

void Engine::resolve(int a, int b) {
  ++pairChecks;
  // Distances in Q8 (positions are Q12). Neighbour cells keep |d| < 16 px so
  // the squares below stay far from int32 overflow.
  const int32_t dx = (x_[a] - x_[b]) >> 4;
  const int32_t dy = (y_[a] - y_[b]) >> 4;
  const int32_t d2 = dx * dx + dy * dy;
  if (d2 >= d2Q16_) return;
  ++pairContacts;
  touched_[a] = touched_[b] = 1;

  int32_t nx, ny, dist;  // unit normal (Q8), pointing from b to a
  if (d2 == 0) {
    nx = (rnd() & 1) ? 256 : -256;
    ny = 0;
    dist = 0;
  } else {
    dist = (int32_t)isqrt32((uint32_t)d2);
    if (dist < 1) dist = 1;
    nx = (dx * 256) / dist;
    ny = (dy * 256) / dist;
  }

  // 1) Separate: move each particle half of the overlap along the normal.
  //    Equal and opposite moves, so the pair's centre of mass never drifts.
  const int32_t ov = twoRQ8_ - dist;
  const int32_t cx = (nx * ov) / 512;
  const int32_t cy = (ny * ov) / 512;
  x_[a] += cx * 16;
  y_[a] += cy * 16;
  x_[b] -= cx * 16;
  y_[b] -= cy * 16;

  // 2) Bounce: equal-mass impulse along the normal, only if approaching.
  const int32_t dvx = vx_[a] - vx_[b];
  const int32_t dvy = vy_[a] - vy_[b];
  const int32_t vn = (dvx * nx + dvy * ny) / 256;
  if (vn < 0) {
    const int32_t e = (-vn < REST_Q12) ? 0 : E_PART_Q8;  // resting contacts: no bounce
    const int32_t j = ((256 + e) * (-vn)) / 512;
    const int32_t jx = (j * nx) / 256;
    const int32_t jy = (j * ny) / 256;
    vx_[a] += jx;
    vy_[a] += jy;
    vx_[b] -= jx;
    vy_[b] -= jy;

    // 3) Contact friction: damp the sliding speed (tangent t = (-ny, nx)).
    const int32_t vt = (-dvx * ny + dvy * nx) / 256;
    const int32_t ft = (vt * FRIC_Q8) / 512;
    const int32_t tx = (-ny * ft) / 256;
    const int32_t ty = (nx * ft) / 256;
    vx_[a] -= tx;
    vy_[a] -= ty;
    vx_[b] += tx;
    vy_[b] += ty;
  }
}

}  // namespace dust
