# Hardware

This describes the schematic in `hardware/schematic/Tracker_Schematic.pdf` (2026-11-10). Known schematic issues are tracked in [open-items.md](open-items.md).

## Block overview

- **USB-C** (GCT USB4105-GF-A, USB 2.0 only) with 5.1 kΩ on each CC pin. VBUS passes a USBLC6-2SC6 ESD array at the connector, then an SS12 Schottky diode, into the charger. USB D+/D- go straight to the ESP32-S3's native USB, with no bridge chip.
- **BQ24074 charger** with power path. Its output (VSYS) runs the board whether or not USB is present.
- **TLV75733P LDO** (1 A, WSON-6 with thermal pad) makes 3V3 from VSYS. A slide switch drives the LDO's EN pin (common to EN, throws to VSYS and GND), so the switch carries almost no current. With it off, the charger, fuel gauge and GNSS coin-cell backup stay alive.
- **ESP32-S3-WROOM-1U-N8R8** in the middle.
- **Shared SPI bus**: BNO085 IMU and Wio-SX1262 LoRa module.
- **Second SPI host (SPI3)**: the ST7789 display, on its own pins so a screen redraw never holds the bus the IMU and radio use.
- **I2C bus**: BMP390 (0x76) and MAX17048 (0x36), plus 1x4 headers for I2C and debug UART.
- **UART1**: MAX-M10S GNSS, plus its PPS output.
- **SDMMC 4-bit**: microSD socket (push-pull, no card detect).

## Pin map

The WROOM-1 module only brings out GPIO0-21 and GPIO35-48. On the N8R8, the octal PSRAM uses GPIO35-37 inside the module, so those pads must stay unconnected. Every other usable GPIO is assigned; there are no spare pins.

| GPIO | Function | Notes |
|---|---|---|
| 0 | BOOT button | Strapping pin, 10 k pull-up |
| 1 | SD D3 | 10 k pull-up |
| 2 | SD D2 | 10 k pull-up |
| 3 | LoRa NSS | 4.7 k pull-up. JTAG strapping pin, ignored unless its eFuse is burned |
| 4 | User button 1 | 10 k pull-up, active low |
| 5 | User button 2 | 10 k pull-up, active low |
| 6 | Display SCK | SPI3 through the GPIO matrix |
| 7 | UART1 TX to GPS RXD | Net name GPS_RX |
| 8 | LoRa DIO1 | Interrupt |
| 9 | LoRa BUSY | |
| 10 | LoRa RF_SW | Drive as RXEN; DIO2 switches TX inside the module |
| 11 | SPI MOSI | IMU and LoRa |
| 12 | SPI SCK | IMU and LoRa |
| 13 | SPI MISO | IMU and LoRa |
| 14 | IMU CS | |
| 15 | UART1 RX from GPS TXD | Net name GPS_TX |
| 16 | GPS PPS | Timestamps IMU samples |
| 17 | I2C SCL | 4.7 k pull-up |
| 18 | I2C SDA | 4.7 k pull-up |
| 19 | USB D- | Fixed |
| 20 | USB D+ | Fixed |
| 21 | IMU INT | 10 k pull-up |
| 35-37 | Not connected | Used by octal PSRAM |
| 38 | Display backlight | LEDC PWM |
| 39 | SD CLK | No pull-up |
| 40 | SD CMD | 10 k pull-up |
| 41 | SD D0 | 10 k pull-up |
| 42 | SD D1 | 10 k pull-up |
| 43 | UART0 TX | Debug header |
| 44 | UART0 RX | Debug header |
| 45 | Display DC | Strapping pin; push-pull output only, nothing may pull it up |
| 46 | Display MOSI | Strapping pin; held low by the internal pull-down at reset |
| 47 | IMU WAKE (PS0) | 10 k pull-up so the IMU boots in SPI mode |
| 48 | IMU RST | |
| EN | Reset | 10 k pull-up, 1 µF to GND. Also drives LoRa NRST and display RES |

Tied off instead of using a GPIO: display CS to GND, display RES to EN, LoRa NRST to EN, GNSS RESET_N to a test point only (reset the GNSS over UART if needed).

Inputs that can wake the S3 from deep sleep must be on GPIO0-21. Buttons, IMU INT, LoRa DIO1 and GPS PPS all are.

## Strapping pins

GPIO0, 3, 45 and 46 are read at reset.

- GPIO45 read high at reset selects 1.8 V flash and the board will not boot. The display's DC line is on 45, so the display module must not have a pull-up on DC. Check each module with a meter.
- GPIO46 must be low at reset. The display's MOSI is on 46, which is fine because the display input is high-impedance.
- GPIO3 has the LoRa NSS pull-up. That is fine; the JTAG strap is ignored unless its eFuse is burned.

## Power

| Setting | Value |
|---|---|
| Charger input limit (ILIM) | 1.5 kΩ, about 1 A |
| Charge current (ISET) | 1.8 kΩ, about 500 mA. Use packs of 500 mAh or more |
| Termination (ITERM) | Open, 10% of charge current (about 50 mA) |
| Safety timers (TMR) | Open, datasheet defaults (30 min precharge, 5 h fast charge) |
| Battery thermistor (TS) | Fixed 10 kΩ, no NTC |
| CE / EN1 / EN2 | CE low, EN1 low, EN2 high (ILIM-programmed input limit) |
| CHG and PGOOD LEDs | Powered from VSYS, so they draw nothing on battery alone |

Peak 3V3 load is roughly 500-600 mA if WiFi and LoRa never transmit at the same time (about 700 mA if everything peaks at once). Typical logging is 150-250 mA. On USB the LDO can dissipate about 0.5 W, which is why it needs a thermal-pad package and a copper pour with vias.

Bulk capacitance: 47 µF + 10 µF + 100 nF on the LDO output, 10 µF + 100 nF at the ESP32's 3V3 pin, 100 µF ceramic at the LoRa module's VCC, about 10 µF at the SD socket. Ceramic capacitors lose much of their value under DC bias in small packages, so the large values use 0805 or 1206.

## GNSS RF path

From the antenna:

```
Edge SMA --+-- C20 (47 pF C0G, DC block) -- SAW filter -- RF_IN (MAX-M10S)
           |
           L2 (27 nH RF inductor) -- R26 (10 Ω) -- VCC_RF, with 10 nF decoupling
```

L2 feeds DC up the coax to the active antenna's amplifier. R26 limits current if the antenna or cable shorts. The SAW filter keeps the board's own LoRa and WiFi signals out of the receiver. The MAX-M10S also has an internal SAW and LNA.

SAW filter: Qualcomm (RF360) B39162B4327P810, 1.4 x 1.1 mm, covers GPS, BeiDou, Galileo and GLONASS, about 1-1.4 dB loss, 50 Ω, no matching parts. Pin 1 input, pin 4 output, pins 2/3/5 ground.

L2 must be an RF-grade inductor (Murata LQG15HS27NJ02D) with self-resonance well above 1.6 GHz.

The GNSS module's VCC and V_IO are on 3V3, VIO_SEL is open, SAFEBOOT and RESET go to test points (TP2 and TP1), and V_BCKP comes from the CR2032.

## PCB

4 layers: signals and RF on top, solid ground on layer 2 (no routing), 3V3 and VSYS pours on layer 3, signals on the bottom. All SMD parts on the top side.

- The 50 Ω GNSS trace is about 0.35 mm wide on JLC's impedance-controlled 4-layer stackups; use their calculator for the exact width.
- Keep every layer except ground clear under the GNSS module, SAW filter and RF trace.
- Put the GNSS module at the board edge next to its SMA, the LoRa module at the opposite edge, and the ESP32 between them.
- Put the USBLC6 right at the USB connector with a short, via-heavy ground.
- Hand-soldered parts: edge SMA, display, headers, battery connector, coin-cell holder.
