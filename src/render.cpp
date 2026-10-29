#include "render.h"

#include <math.h>
#include <string.h>

namespace gfx {

namespace {

typedef uint32_t __attribute__((may_alias)) u32a;

constexpr int W = SCREEN_W;
constexpr int H = SCREEN_H;

uint16_t g_rowColor[H];       // background gradient, one colour per row
uint16_t g_palette[32];       // particle colour by speed (cold -> hot)
int8_t g_dx[64], g_dy[64];    // pixels of one particle disc, relative to its centre
int g_discN = 0;

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int absi(int v) { return v < 0 ? -v : v; }

uint8_t lerp8(int a, int b, int t, int tmax) { return (uint8_t)(a + (b - a) * t / tmax); }

// 3x5 pixel font: each glyph is 5 rows of 3 bits (MSB = left pixel).
const uint8_t* glyph(char ch) {
  static const uint8_t digits[10][5] = {
      {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {6, 1, 2, 4, 7}, {6, 1, 2, 1, 6}, {5, 5, 7, 1, 1},
      {7, 4, 6, 1, 6}, {3, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 6}};
  static const uint8_t F[5] = {7, 4, 6, 4, 4}, I[5] = {7, 2, 2, 2, 7}, X[5] = {5, 5, 2, 5, 5},
                       E[5] = {7, 4, 6, 4, 7}, D[5] = {6, 5, 5, 5, 6}, T[5] = {7, 2, 2, 2, 2},
                       L[5] = {4, 4, 4, 4, 7}, P[5] = {6, 5, 6, 4, 4}, S[5] = {3, 4, 2, 1, 6},
                       G[5] = {3, 4, 5, 5, 3};
  if (ch >= '0' && ch <= '9') return digits[ch - '0'];
  switch (ch) {
    case 'F': return F;  case 'I': return I;  case 'X': return X;  case 'E': return E;
    case 'D': return D;  case 'T': return T;  case 'L': return L;  case 'P': return P;
    case 'S': return S;  case 'G': return G;
    default: return nullptr;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
void Canvas::pixel(int x, int y, uint16_t c) const {
  if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) px[y * W + x] = c;
}

void Canvas::fillRect(int x, int y, int w, int h, uint16_t c) const {
  int x0 = clampi(x, 0, W), x1 = clampi(x + w, 0, W);
  int y0 = clampi(y, 0, H), y1 = clampi(y + h, 0, H);
  for (int yy = y0; yy < y1; ++yy)
    for (int xx = x0; xx < x1; ++xx) px[yy * W + xx] = c;
}

void Canvas::line(int x0, int y0, int x1, int y1, uint16_t c) const {  // Bresenham
  int dx = absi(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -absi(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    pixel(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void GraphHistory::push(float lin_g, float agitation) {
  int l = (int)(lin_g / 4.0f * 255.0f);
  int a = (int)(agitation * 255.0f);
  lin[head] = (uint8_t)clampi(l, 0, 255);
  agit[head] = (uint8_t)clampi(a, 0, 255);
  head = (head + 1) % SCREEN_W;
}

// ---------------------------------------------------------------------------
void init(const dust::Engine& e) {
  // Background: deep navy at the top fading to teal at the bottom ("liquid").
  for (int y = 0; y < H; ++y)
    g_rowColor[y] = rgb(lerp8(6, 10, y, H - 1), lerp8(12, 48, y, H - 1), lerp8(34, 66, y, H - 1));

  // Particle colour by speed: slow = blue, medium = cyan, fast = warm white.
  for (int i = 0; i < 32; ++i) {
    if (i < 16)
      g_palette[i] = rgb(lerp8(80, 120, i, 15), lerp8(140, 225, i, 15), lerp8(215, 255, i, 15));
    else
      g_palette[i] = rgb(lerp8(120, 255, i - 16, 15), lerp8(225, 245, i - 16, 15), lerp8(255, 200, i - 16, 15));
  }

  // Disc pixels for the current particle radius.
  float r = e.radiusPx() + 0.45f;
  int R = (int)ceilf(r);
  g_discN = 0;
  for (int dy = -R; dy <= R; ++dy)
    for (int dx = -R; dx <= R; ++dx)
      if ((float)(dx * dx + dy * dy) <= r * r && g_discN < 64) {
        g_dx[g_discN] = (int8_t)dx;
        g_dy[g_discN] = (int8_t)dy;
        ++g_discN;
      }
}

void drawBeaker(const Canvas& c, const dust::Engine& e) {
  for (int y = 0; y < H; ++y) {
    uint32_t v = (uint32_t)g_rowColor[y] | ((uint32_t)g_rowColor[y] << 16);
    u32a* row = (u32a*)(c.px + y * W);
    for (int k = 0; k < W / 2; ++k) row[k] = v;
  }
  const int32_t* x = e.x();
  const int32_t* y = e.y();
  const int32_t* vx = e.vx();
  const int32_t* vy = e.vy();
  for (int i = 0; i < e.count(); ++i) {
    int cx = (x[i] + dust::ONE / 2) >> dust::Q;
    int cy = (y[i] + dust::ONE / 2) >> dust::Q;
    int s = (absi(vx[i]) + absi(vy[i])) >> 9;
    uint16_t col = g_palette[s > 31 ? 31 : s];
    for (int k = 0; k < g_discN; ++k) {
      int px = cx + g_dx[k], py = cy + g_dy[k];
      if ((unsigned)px < (unsigned)W && (unsigned)py < (unsigned)H) c.px[py * W + px] = col;
    }
  }
}

void drawGraph(const Canvas& c, const GraphHistory& h) {
  const uint16_t bg = rgb(8, 10, 18), grid = rgb(40, 48, 64);
  for (int i = 0; i < W * H; ++i) c.px[i] = bg;
  for (int g = 1; g <= 3; ++g) {  // horizontal lines at 1 g, 2 g, 3 g
    int yy = (H - 1) - g * 255 / 4 * (H - 1) / 255;
    c.fillRect(0, yy, W, 1, grid);
    char t[3] = {(char)('0' + g), 'G', 0};
    drawText(c, 3, yy - 8, t, 1, rgb(110, 120, 140));
  }
  int prevL = 0, prevA = 0;
  for (int k = 0; k < W; ++k) {
    int idx = (h.head + k) % W;
    int yl = (H - 1) - h.lin[idx] * (H - 1) / 255;
    int ya = (H - 1) - h.agit[idx] * (H - 1) / 255;
    if (k > 0) {
      c.line(k - 1, prevA, k, ya, rgb(255, 150, 40));   // agitation (orange)
      c.line(k - 1, prevL, k, yl, rgb(80, 220, 255));   // shaking strength (cyan)
    }
    prevL = yl;
    prevA = ya;
  }
}

void drawText(const Canvas& c, int x, int y, const char* s, int scale, uint16_t color) {
  for (; *s; ++s, x += 4 * scale) {
    const uint8_t* g = glyph(*s);
    if (!g) continue;
    for (int row = 0; row < 5; ++row)
      for (int col = 0; col < 3; ++col)
        if (g[row] & (4 >> col)) c.fillRect(x + col * scale, y + row * scale, scale, scale, color);
  }
}

void drawBadge(const Canvas& c, const char* text) {
  int n = 0;
  while (text[n]) ++n;
  c.fillRect(4, 4, n * 12 + 6, 24, rgb(0, 0, 0));
  drawText(c, 7, 7, text, 3, rgb(255, 255, 255));
}

void drawBattery(const Canvas& c, int percent, bool usbPowered) {
  const int bx = W - 26, by = 6;
  const uint16_t outline = rgb(190, 200, 210);
  c.fillRect(bx, by, 18, 9, outline);
  c.fillRect(bx + 18, by + 2, 2, 5, outline);
  c.fillRect(bx + 1, by + 1, 16, 7, rgb(0, 0, 0));
  int level = usbPowered ? 100 : clampi(percent, 0, 100);
  uint16_t col = usbPowered ? rgb(80, 220, 255)
                            : (level > 40 ? rgb(70, 220, 110) : (level > 15 ? rgb(250, 190, 40) : rgb(240, 60, 50)));
  int w = (14 * level + 99) / 100;
  if (level > 0 && w < 1) w = 1;
  c.fillRect(bx + 2, by + 2, w, 5, col);
}

void drawWarning(const Canvas& c) { c.fillRect(8, 8, 8, 8, rgb(240, 40, 40)); }

}  // namespace gfx
