# Tracker

A battery-powered GPS logger and LoRa tracker built around an ESP32-S3. It is the third revision of the 2019 GPS Board (V2) and is also called GPS Logger V3.

The board logs position, motion and pressure to a microSD card, shows status on a 2.0" colour display, and can send position reports over LoRa (915 MHz) and WiFi/BLE. With 8 MB of PSRAM it is also meant as a platform for small on-device ML models (activity classification, crash detection, vibration anomaly detection).

## Status

| Area | State |
|---|---|
| Schematic | Updated 2026-11-10; one known fix outstanding, listed in [docs/open-items.md](docs/open-items.md) |
| PCB layout | In progress (4-layer, single-sided SMD). Layout files are kept out of this repo for now |
| Prototypes | 5 boards, hand-assembled with a stencil and solder paste |
| Firmware | Not started in this repo; pin definitions are in [firmware/include/pins.h](firmware/include/pins.h) |

## Hardware at a glance

| Function | Part |
|---|---|
| MCU | ESP32-S3-WROOM-1U-N8R8 (8 MB flash, 8 MB octal PSRAM, U.FL antenna) |
| GNSS | u-blox MAX-M10S, active antenna on an edge-mount SMA, external SAW filter |
| LoRa | Seeed Wio-SX1262 (SX1262, TCXO, 22 dBm, 902-928 MHz) |
| IMU | CEVA BNO085 over SPI |
| Barometer | Bosch BMP390 over I2C |
| Display | 2.0" 240 x 320 ST7789 IPS module (ZJY200S0800TG01 panel) on its own SPI bus |
| Storage | microSD, 4-bit SDMMC |
| Power | USB-C, TI BQ24074 charger with power path, MAX17048 fuel gauge, TLV75733P 3.3 V LDO switched on its enable pin, single-cell LiPo on JST PH |
| GNSS backup | CR2032 coin cell on V_BCKP |

## Repository layout

```
docs/
  hardware.md        Block diagram in words, pin map, power, buses, strapping pins
  antennas.md        Three-radio antenna plan and coexistence rules
  firmware-notes.md  Library choices and configuration for each peripheral
  decisions.md       Why the design is the way it is
  open-items.md      Pending changes and things to check
firmware/
  include/pins.h     GPIO assignments matching the schematic
hardware/
  schematic/Tracker_Schematic.pdf
```

Firmware projects (bring-up tests, the logger itself) will go under `firmware/`.
