# hwtest

Bring-up firmware for the Tracker board. On boot it tests each subsystem, prints a PASS/FAIL report over USB serial, shows the summary on the display, then switches to a live view.

See [docs/bring-up.md](../../docs/bring-up.md) for the full first-power-on procedure.

## Build and flash

Requires [PlatformIO](https://platformio.org/) (VS Code extension or CLI).

```
cd firmware/hwtest
pio run -t upload
pio device monitor
```

On a fresh board the S3 may need to be put in download mode first: hold BOOT, press and release RESET, release BOOT, then upload. Press RESET afterwards to run the firmware.

Pins come from [`firmware/include/pins.h`](../include/pins.h). Library versions are pinned in `platformio.ini`.

## What it checks

| Test | Pass condition |
|---|---|
| Flash, PSRAM | 8 MB flash; 8 MB PSRAM with a 4 MB write/verify pattern |
| Display | Init succeeds. Marked LOOK: confirm the colour bars read red, green, blue, white, black |
| Buttons | B1, B2 and BOOT all read released at boot |
| I2C bus | MAX17048 at 0x36 and BMP390 at 0x76 answer |
| Gauge | Cell voltage between 2.5 and 4.35 V (needs a battery: the gauge runs from the cell) |
| Barometer | Pressure 850-1100 hPa, temperature -10 to 60 °C |
| IMU | BNO085 answers over SPI and sends rotation-vector events |
| LoRa | SX1262 initialises (TCXO 1.8 V, DIO2 RF switch) and starts listening at 906.875 MHz |
| GNSS | Valid NMEA sentences at 38400 baud (falls back to 9600 and warns) |
| SD card | Mounts in 4-bit mode, writes and reads back `/hwtest.txt` (falls back to 1-bit and warns) |

## Live view and serial commands

The live view shows GPS satellites, fix and position, PPS pulse count, battery, pressure, IMU quaternion, last LoRa packet, button states and SD status.

| Key | Action |
|---|---|
| `h` | Help |
| `r` | Rerun all tests |
| `t` | Send one LoRa packet at 10 dBm (antenna must be connected) |
| `g` | Toggle raw NMEA passthrough from the GNSS |
| `b` | Backlight sweep (checks PWM on GPIO38) |
| `s` | Append a line to `/hwtest.txt` on the SD card |

Two boards running hwtest can talk to each other: press `t` on one and the other prints the packet with RSSI and SNR.
