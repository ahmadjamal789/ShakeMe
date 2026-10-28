# Shakedown (working title)

A handheld, battery-powered display that behaves like a solute in a solvent. Left at rest, the particles on screen settle to the bottom. Shake or move it, and they are kicked up and spread through the whole volume. When the motion stops, they slowly resettle.

Built for **Hack Club, Week 4: Displays Week** ("Design with Displays").

<p align="center">
  <img width="1174" height="664" alt="Screenshot from 2026-10-28 15-06-47" src="https://github.com/user-attachments/assets/a2651c86-c9db-4ae4-9d36-6ff9f79583f7" />

  <br><em>The finished device.</em>
</p>


## The idea

A motion sensor measures how hard the device is being disturbed, and that measurement drives a particle simulation drawn on a color TFT.

- **At rest:** gravity wins and the particles settle into a layer, like sediment.
- **Disturbed:** the acceleration reading becomes an "agitation" value that gives the particles random kicks, so they disperse rapidly.
- **After the disturbance:** agitation decays over time and the particles slowly resettle.

## Status

- [x] Concept and parts chosen
- [x] Schematic drawn in KiCad
- [x] Custom symbols, footprints and 3D models for the display, IMU, charger and battery
- [ ] PCB layout (in progress)
- [ ] Firmware
- [ ] Enclosure

## Hardware

| Part | Choice | Notes |
|---|---|---|
| Microcontroller | Raspberry Pi Pico (RP2040) | Powered through VSYS |
| Display | ST7789 2.0" 240x320 SPI TFT | 3.3 V logic, 8-pin header |
| Motion sensor | MPU6050 breakout (8-pin) | I2C, 3.3 V, on-board regulator and pull-ups |
| Battery | 3.7 V LiPo, 500 to 1000 mAh, with protection board | Pouch cell no larger than the Pico (21 x 51 mm) |
| Charger | TP4056 Type-C module (with protection) | Charges the battery over USB-C |
| Power switch | Slide switch | Between the battery and the Pico |
| Diode | Schottky, SOD-123 | Battery to VSYS, so USB and battery don't fight |
| Controls | Two tactile buttons | Mode and spare |
| Passives | 10 uF bulk capacitor on VSYS, 100 nF decoupling on 3.3 V | Plus backlight MOSFET and resistors if the display needs them |

### Power

```
USB-C -> TP4056 -> LiPo (protected) -> slide switch -> Schottky diode -> Pico VSYS
                                                                        |
                                              Pico 3V3(OUT) -> display, IMU
```

- The TP4056 module charges the cell from its own USB-C port. Switch the device off while charging.
- The Pico's own regulator makes the 3.3 V rail, so no separate regulator is used.
- VBUS is left unconnected. The Pico's USB port powers it internally when plugged in.

### Pin assignments

Planned mapping. Update this table to match the final schematic.

| Signal | Pico pin |
|---|---|
| Display DC / CS / SCL / SDA / RES / BLK | GP0 / GP1 / GP2 / GP3 / GP4 / GP5 |
| IMU SDA / SCL | See schematic (I2C pair) |
| IMU AD0 | GND (address 0x68) |
| Buttons | See schematic |

## PCB design

All design work was done in KiCad. The three modules that have no stock library parts (display, IMU, charger) and the LiPo cell outline use custom symbols, footprints and 3D models, kept in `hardware/DisplaysWeek/`.

### Schematic

<p align="center">
  <img width="681" height="350" alt="Screenshot from 2026-10-28 21-36-41" src="https://github.com/user-attachments/assets/9ddae274-d769-4062-87e2-89ad8a27a62e" />

  <br><em>Figure 1. Full schematic.</em>
</p>

The schematic is split into five blocks:

- **Power:** battery connector, TP4056 charger, slide switch, Schottky diode to VSYS, 10 uF bulk capacitor.
- **Controller:** Pico, with the 3.3 V rail taken from its 3V3(OUT) pin and decoupled with 100 nF.
- **Display:** ST7789 on hardware SPI, with the backlight driven directly or through a MOSFET depending on the module.
- **Motion:** MPU6050 on I2C.
- **Controls:** two tactile buttons.

### Board layout

<p align="center">
  <img width="455" height="296" alt="Screenshot from 2026-10-28 15-05-07" src="https://github.com/user-attachments/assets/1117d877-8ce4-4c5e-8c20-d95d43231b1d" />

  <br><em>Figure 2. Full board layout, all layers visible.</em>
</p>

<p align="center">
<img width="611" height="407" alt="Screenshot from 2026-10-28 21-35-10" src="https://github.com/user-attachments/assets/babfcf34-b631-4bcb-8d33-f5ac0153e4e8" />
</p>

- **Board:** 2 layers, [width] x [height] mm.
- **Placement:** [describe which parts sit on the front and which on the back once the layout is final].
- **Pin tails:** through-hole header pins come out on the opposite face. Each header has a keep-out rule area on the other side, so nothing sits where its tails land. The display header's tails are kept clear of the Pico, whose underside has exposed test pads, and of the battery.
- **Battery:** the cell is a courtyard-only footprint with no pads. No tails and no tall parts sit under it.
- **Traces:** about 0.5 mm for VSYS and the battery path, about 0.25 mm for signals, with a GND pour on both sides and stitching vias.
- **IMU placement:** kept away from the charger, which warms up while charging.

### 3D render

| Front | Back |
|---|---|
| <img width="572" height="392" alt="Screenshot from 2026-10-28 21-33-01" src="https://github.com/user-attachments/assets/5b6b2608-125a-4d47-8ad6-cde5cf29b210" /> | <img width="565" height="390" alt="Screenshot from 2026-10-28 21-33-42" src="https://github.com/user-attachments/assets/8a04108e-cc4c-471b-be6a-79b3a34fcd4f" /> |
| *Figure 5. 3D render, front.* | *Figure 6. 3D render, back.* |

Rendered with KiCad's 3D viewer using the custom STEP models.


### Final 3D model

<p align="center">
 <img width="572" height="392" alt="Screenshot from 2026-10-28 21-33-01" src="https://github.com/user-attachments/assets/9cd4a7d8-2ee6-46af-9dd5-026b473c3b67" />
  <br><em>Figure 7. Final 3D model of the assembled device.</em>
</p>

### Manufacturing files

- Gerbers and drill files: `hardware/gerbers/` (add when generated)
- Bill of materials: `hardware/bom.csv` (add when generated)

## Repository layout (suggested)

```
hardware/
  kicad/                  KiCad project (schematic and PCB)
  DisplaysWeek/           custom symbol, footprint and 3D libraries
  gerbers/                manufacturing files
firmware/                 Pico firmware
docs/
  images/                 pictures used in this README
README.md
```


## Firmware plan

- C++ with the Arduino-Pico core and PlatformIO.
- Display: TFT_eSPI or LovyanGFX over hardware SPI.
- IMU: any MPU6050 library; subtract gravity and take the acceleration magnitude.
- Physics loop each frame:
  1. Read acceleration and low-pass filter it.
  2. Update agitation: `E = max(E * decay, |a|)`.
  3. For each particle: apply gravity, add a random kick scaled by `E`, apply drag, bounce off the walls.
- Optional: use the gravity vector so particles slosh when the device is tilted.

## Design notes and known issues

- **Through-hole pin tails:** header pins come out on the opposite side of the board. Keep them clear of other parts, and never let them touch the battery or the Pico's underside test pads. Clip them flush after soldering and cover with Kapton tape.
- **Backlight:** some ST7789 modules take BLK as a logic input, others use it as the LED cathode. Test the module before deciding whether the MOSFET is needed.
- **LiPo polarity:** many cells use the opposite polarity to the standard JST connector. Check the cell's red and black wires against the connector before plugging in.
- **Module sizes:** the footprints use typical dimensions. Measure the real modules before ordering boards.

## Safety

LiPo cells can catch fire if shorted, punctured, over-charged or over-discharged. Use a cell with a protection board, never charge it unattended while testing, and stop using it if the pouch swells.

## Credits

Built for Hack Club Displays Week. Designed in KiCad.
