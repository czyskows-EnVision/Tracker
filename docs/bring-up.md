# Bring-up procedure

For the first hand-assembled boards. Do the steps in order and stop at the first one that fails.

## 1. Before power

- Inspect under magnification: solder bridges on the BQ24074 (VQFN-16), TLV75733P (WSON-6), MAX17048 (TDFN-8), BMP390 and BNO085 (LGA), and the SAW filter. Check the orientation of the LEDs, D1, the USBLC6 and BT1.
- With a meter, measure resistance to GND on VBUS_IN, VSYS, BATT and 3V3. Anything under about 100 Ω points to a short.
- Leave the coin cell, battery, display and SD card out for now.

## 2. First power

Power from USB through a USB power meter, or from a bench supply on the battery connector (3.8-4.0 V, current limit about 200 mA).

| Check | Expected |
|---|---|
| PGOOD LED (USB only) | On |
| VSYS | About 4.4 V on USB; battery voltage on battery |
| 3V3, switch on | 3.3 V |
| 3V3, switch off | 0 V (the LDO's EN is pulled to GND) |
| Current, switch on, no firmware | Tens of mA. A reading near the limit means a short or a part in backwards |

Then with a LiPo connected and USB plugged in, the CHG LED should light while charging.

## 3. USB and first flash

- Plug into a computer. A blank S3 shows up as a USB serial / JTAG device.
- If it doesn't: hold BOOT, press and release RESET, release BOOT. If it still doesn't appear, check D+/D- through the USBLC6 and the strapping pins: GPIO45 and GPIO46 must read low at reset, GPIO0 high unless BOOT is held.
- Flash `firmware/hwtest` (see [its README](../firmware/hwtest/README.md)).

## 4. Run hwtest

Fit the display, SD card, coin cell, battery and all three antennas, then open the serial monitor and press RESET.

Never transmit LoRa without an antenna connected. hwtest only transmits when you press `t`.

| Result | What to check |
|---|---|
| PSRAM FAIL | `board_build.arduino.memory_type = qio_opi` in `platformio.ini`; module must be the N8R8 |
| Display FAIL or blank screen | Backlight on GPIO38 (try `b`); DC on GPIO45 and MOSI on GPIO46; CS tied to GND; 3V3 at the header |
| Colours wrong | Red/blue swapped: set `rgb_order = true` in `display.h`. Inverted colours: toggle `invert` |
| Button FAIL | A button reading pressed at boot is shorted or its pull-up is missing |
| I2C bus FAIL | 4.7 k pull-ups on GPIO17/18; BMP390 CSB high and SDO to GND; gauge needs a battery |
| Gauge FAIL | Battery connected? CELL and VDD on BATT, CTG and QSTRT to GND |
| Barometer FAIL | Chip ID should be 0x60; check SDI on SDA and the LGA joints |
| IMU FAIL | PS1 high and PS0 (GPIO47) high at reset; BOOTN pull-up; CS on GPIO14, INT on GPIO21, RST on GPIO48 |
| LoRa FAIL, error -2 | No SPI reply: NSS on GPIO3, BUSY on GPIO9, VCC and the 100 µF cap |
| LoRa FAIL, error -707 | TCXO or clock problem: check the TCXO voltage setting for the module |
| GNSS FAIL | TX/RX swapped (GPIO7 to GPS RXD, GPIO15 from GPS TXD); VCC and V_IO on 3V3 |
| GNSS warns at 9600 baud | The module was configured for 9600; set `GPS_BAUD` or reconfigure it |
| SD 1-bit only | D1, D2 or D3 (GPIO42, 2, 1) open or missing their pull-ups |
| SD FAIL | No card, or CLK/CMD/D0 (GPIO39, 40, 41) |

## 5. Live checks

- **GNSS:** take the board outside or to a window. Satellites should appear within a minute and a fix within a few minutes on a cold start. PPS should count once per second after the fix.
- **Coin cell:** switch off for a few minutes, switch on, and the fix should come back in seconds (hot start).
- **IMU:** rotate the board and watch the quaternion change.
- **Buttons:** each press shows on the screen.
- **LoRa:** with two boards, press `t` on one and check RSSI and SNR on the other. Then check GNSS C/N0 with LoRa transmitting (see [antennas.md](antennas.md)).
- **Charging:** on USB with a battery, the gauge's percentage should rise over time.
