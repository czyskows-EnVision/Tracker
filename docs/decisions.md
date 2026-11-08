# Design decisions

Why the board is the way it is. Newest decisions last within each section.

## From V2 to V3

- **ESP32-S3 instead of ESP32.** Native USB drops the CP2102 bridge and the three-transistor auto-reset circuit. The S3 also has vector instructions for ML.
- **Real charger with power path** (BQ24074) instead of the V2's opto and P-FET arrangement, and a fuel gauge (MAX17048) instead of a resistor divider.
- **BMP390** replaces the LPS25HB, which is end of life.
- **MAX-M10S** GNSS for multi-constellation reception and PPS.

## MCU module

- **WROOM-1U (U.FL) with an external 2.4 GHz antenna**, so the board can live in an enclosure and get better WiFi/BLE range.
- **N8R8 instead of N8.** 8 MB of octal PSRAM for edge ML and display buffers. Cost: GPIO35-37 are used internally, which forced the display onto GPIO6/46 and LoRa reset onto EN.
- GPIO33 and 34 are not available on the WROOM-1 at all. An early pin map used them by mistake.

## Display

- **2.0" 240 x 320 ST7789 IPS** replaces the I2C OLED. Full colour, a couple of inches, cheap generic modules (a few dollars on AliExpress versus about $20 from Adafruit or SparkFun).
- **SPI, not I2C.** A full 320 x 240 frame at 16-bit colour is about 150 KB, several seconds per redraw at 400 kHz I2C.
- **Its own SPI host (SPI3)**, so redraws never hold the bus the IMU and LoRa radio depend on.
- **Soldered directly through header pins**, no ribbon cable, mounted on M2 standoffs so the joints don't carry load.
- **CS tied low, RES tied to EN** to save pins; the display is the only device on its bus.
- A JDI memory-in-pixel display was considered for sunlight readability, but it is limited to 8 colours and availability is poor.

## Storage

- **Push-pull microSD socket without card detect.** Card detect doesn't protect against pulling a card mid-write; frequent flushes do. Freed a GPIO.

## GNSS

- **Active antenna on an edge-mount SMA**, following SparkFun's MAX-M10S breakout.
- **Antenna feed** follows the active-antenna circuit in the MAX-M10S Integration Manual (UBX-20053088): RF inductor (not a ferrite bead) plus a series resistor, DC block, SAW filter. The manual's LNA_EN-controlled antenna power switch was left out; antenna power is always on.
- **SAW filter: Qualcomm B39162B4327P810** (1.4 x 1.1 mm, stocked at DigiKey) instead of the Murata SAFFB1G56KB0F0A (1.1 x 0.9 mm, not carried by DigiKey and harder to hand-place).
- **CR2032 backup** on V_BCKP rather than a diode feed from the LiPo.
- **RESET_N left open.** Tying it to EN would discard tracking state on every ESP32 reset.

## LoRa

- **Seeed Wio-SX1262**: TCXO, 22 dBm, internal RF switch on DIO2, its own U.FL, FCC modular grant. Same module as the Meshtastic-supported XIAO ESP32S3 kit.
- **Reset tied to EN** to save a GPIO. Trade-off: a software restart doesn't reset the radio, and a wedged radio needs the RESET button or a power cycle.

## Power

- **LDO, not a buck-boost.** A quiet supply next to the GNSS receiver is worth more than the last bit of battery a buck-boost would recover. 1 A is ample (peak about 600 mA with radios scheduled).
- **TLV75733P in WSON-6 with a thermal pad**, for its enable pin (so the slide switch carries microamps) and to handle about 0.5 W on USB.
- **No diode between the charger output and the LDO**; it would waste 0.3-0.5 V of battery headroom.
- **USB ESD array VBUS pin on the connector side** of the input diode, so surges are clamped where they enter.

## PCB and assembly

- **4 layers** for a sensible 50 Ω GNSS trace, an unbroken ground plane under the RF section, and better LDO heat spreading.
- **All SMD parts on one side.** At JLC, double-sided assembly needs Standard PCBA, roughly $70-90 more per order.
- **Prototypes hand-assembled** with a stencil and solder paste; parts ordered from DigiKey, with a few LCSC-only parts.
