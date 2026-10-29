# Firmware

Brownian-dust display for the Raspberry Pi Pico (RP2040) with an ST7789 240x320 TFT
and an MPU6050.

- **At rest** the particles settle into a layer about 15% of the screen tall.
- **Gentle shake:** they stir randomly for a short while, then settle again.
- **Violent shake:** they scatter through the whole box like a gas, bouncing off the walls
  and each other until their kinetic energy dies off, then they resettle.

Everything runs on the Pico alone. No external libraries are needed.

## Build and upload

1. Install [PlatformIO](https://platformio.org/) (VS Code extension or CLI).
2. Open this `firmware/` folder.
3. Hold **BOOTSEL** while plugging in the Pico, then run **Upload** (`pio run -t upload`).
4. Open the serial monitor at 115200 baud (`pio device monitor`).

The first build downloads the Pico toolchain, which takes a few minutes.

## Before you flash: check `include/config.h`

| Setting | Default | Check against |
|---|---|---|
| `TFT_PIN_SCK/MOSI/CS/DC/RST/BL` | GP2 / 3 / 1 / 0 / 4 / 5 | your schematic. SCK and MOSI must be a valid SPI0 pair |
| `IMU_PIN_SDA/SCL` | GP6 / GP7 | your schematic (I2C1) |
| `IMU_I2C_ADDR` | 0x68 | AD0 low = 0x68, AD0 high = 0x69 |
| `BTN_PIN_MODE/GRAVITY` | GP8 / GP9 | your schematic |
| `BL_ACTIVE_HIGH` | 1 | see "Backlight" below |
| `MAP_S*` | X, -Y, Z | see "Orientation" below |

## First power-up checklist

Do these in order. Each step proves one part works.

1. **Screen flashes red, green, blue**, then shows falling dust. If not, see Troubleshooting.
2. **Serial shows `MPU6050: ok`.** If not, check SDA/SCL, power, and the address.
3. **Orientation check.** Hold the device upright (screen facing you, top edge up). The serial
   line `s=(x, y, z)` should read about `(0, -1.00, 0)`. If the -1 is on another axis or has
   the wrong sign, edit the `MAP_*` lines in `config.h`:
   - `sy` must be about -1 when upright.
   - Tilt the device to the right: `sx` should go negative or positive consistently. If shaking
     left and right moves the particles the wrong way, flip `MAP_SX_SIGN`.
4. **Shake it.** The particles should scatter and then settle. Press the **mode button** to
   see the graph view, and the **gravity button** to switch between fixed gravity (always
   toward the bottom of the screen) and tilt gravity (particles slosh as you tilt).

### Backlight

Some ST7789 modules take BLK as a logic input (high = on). Others use it as the LED cathode.
If the screen is black but the controller is running, try `BL_ACTIVE_HIGH 0`, or drive the
pin through a MOSFET as in the hardware design. Do not drive a bare cathode pin straight
from a GPIO.

## Controls

| Control | Action |
|---|---|
| Mode button | Beaker view <-> graph view (shaking strength in cyan, agitation in orange) |
| Gravity button | Fixed gravity <-> tilt gravity (a "FIXED" / "TILT" label appears briefly) |
| Idle 30 s | Backlight dims. Any motion or button wakes it |

Top-right is a battery icon. Cyan means USB powered. A red square at the top-left means the
IMU is not responding (the firmware keeps retrying).

## How it works

```
 MPU6050 --I2C--> Motion --(accel, kick)--> Engine --> Renderer --> framebuffer --DMA--> ST7789
   ^                 float                  fixed-point      RGB565 in RAM
   120 Hz reads                            120 Hz steps
```

- **Motion** (`motion.cpp`): an accelerometer measures `a_device - g`, so a free particle
  accelerates by the opposite. A slow low-pass filter separates gravity from shaking. The
  shaking part pushes the particles (inertia) and feeds an **agitation** value that jumps up
  instantly and fades over about 1.2 s.
- **Brownian motion:** agitation scales a random velocity kick given to every particle each
  step. Drag keeps the speed bounded, so you get a random walk that dies out as agitation fades.
- **Engine** (`particles.cpp`): about 400 discs in integer fixed-point (the RP2040 has no FPU).
  A uniform grid finds neighbours, overlaps are pushed apart, and collisions exchange momentum
  with restitution below 1, so kinetic energy is lost on every impact. Slow touching particles
  are damped to rest so the settled pile is perfectly still.
- **Renderer** (`render.cpp`): draws into a 150 KB RGB565 framebuffer. Particle colour shows speed
  (blue = slow, white = fast).
- **Display** (`st7789_dma.cpp`): sends the whole frame by DMA, while the CPU runs the physics
  for the next one.

Memory: about 175 KB of the Pico's 264 KB RAM. Expect roughly 25 to 30 fps.

## Tuning ("feel")

All in `config.h`.

| You want | Change |
|---|---|
| Slower, more syrupy settling | raise `DRAG_PER_S` |
| Dust that falls faster | raise `GRAVITY_PX_S2` |
| Shaking has more effect | raise `SHAKE_PX_S2_PER_G` |
| More violent gas phase | raise `KICK_PX_PER_STEP` |
| Gas lasts longer | raise `AGIT_TAU_S` |
| Bouncier walls / particles | raise `E_WALL` / `E_PART` |
| Ignores small bumps | raise `AGIT_DEADBAND_G` |
| Smaller, more numerous particles | raise `PARTICLE_COUNT` (radius shrinks automatically; max about 600) |
| Different pile height | change `SETTLED_FRACTION`, then recalibrate `PACKING` (below) |

After changing particle count or pile height, run `make test` in `test/host` and read the
`SETTLED: pile top = ...%` line. Scale `PACKING` by `target / measured` until they match.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Black screen, no flash | wrong pins, `BL_ACTIVE_HIGH`, or the SPI pins are not a valid SPI0 pair |
| Colours inverted | toggle `TFT_INVERT` |
| Red and blue swapped | set `TFT_MADCTL` to `0x08` |
| Image mirrored or rotated | change `TFT_MADCTL` (0x00, 0xC0, ...) |
| Garbage or noise on the screen | lower `TFT_SPI_HZ` (try 20000000) and keep the SPI wires short |
| `MPU6050: NOT FOUND` | SDA/SCL wiring or swapped, wrong `IMU_I2C_ADDR`, no 3.3 V |
| Particles fall sideways or upward | fix `MAP_*` (see the orientation check) |
| Particles never calm down | the IMU axis map is wrong, so a constant "shake" is being read |
| Low frame rate | enable `SHOW_FPS`, reduce `PARTICLE_COUNT` or `COLLISION_ITERS` |

## Tests (on a PC, no hardware)

```
cd test/host
make test         # physics scenarios with sanitizers: settling, gentle shake, violent shake
make syntax       # compiles every firmware source against stand-in SDK headers
make screenshots  # renders PNGs with the real renderer
```

## Files

```
include/config.h      every setting
src/main.cpp          setup and frame loop
src/particles.*       fixed-point physics engine        (pure C++)
src/motion.*          accelerometer -> physics inputs   (pure C++)
src/render.*          framebuffer drawing               (pure C++)
src/imu_mpu6050.*     MPU6050 over pico-sdk I2C
src/st7789_dma.*      ST7789 over pico-sdk SPI + DMA
src/board.*           backlight, buttons, battery
test/host/            PC tests and screenshot tool
test/mocks/           stand-in SDK headers for the syntax check
```
