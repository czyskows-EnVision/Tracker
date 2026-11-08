# Antennas

The board has three radios and three antennas, all off the board. Where the antennas sit matters much more than where the modules sit.

| Radio | Band | Port | Antenna | Placement |
|---|---|---|---|---|
| GNSS (MAX-M10S) | 1575 MHz L1, receive only | Edge-mount SMA on the board | Active patch, ideally with its own SAW filter ahead of the LNA (Taoglas AGGP.25F, or a budget Waveshare GNSS patch with a U.FL-to-SMA cable). Adafruit #960 puck for outdoor mounting | Top of the enclosure, facing the sky, as far as possible from both transmitting antennas |
| LoRa (SX1262) | 902-928 MHz, 22 dBm | SMA bulkhead, U.FL pigtail | Quarter-wave 915 MHz whip | One end or corner, vertical |
| WiFi / BLE (ESP32-S3) | 2.4 GHz | RP-SMA bulkhead, U.FL pigtail | 2-3 dBi dipole | Opposite end from the LoRa whip, vertical |

## Rules

- **Spacing.** Aim for 15-20 cm or more between the GNSS antenna and each transmitting antenna. The LoRa and WiFi antennas barely affect each other.
- **Blocking, not harmonics.** 915 MHz is outside the GNSS band, and its second harmonic (1830 MHz) misses it. The risk is a strong nearby transmitter overloading the active antenna's amplifier. The on-board SAW filter protects the receiver; an antenna with its own pre-filter protects its amplifier too.
- **Never transmit WiFi and LoRa at the same time.** In anything nonlinear, the two mix to WiFi minus LoRa, roughly 1475-1570 MHz across US channels. That reaches BeiDou B1 (1561 MHz) and sits just below GPS L1. Schedule the two radios in firmware.
- **Make the ports impossible to swap.** RP-SMA for WiFi, SMA for LoRa and GNSS, and label the enclosure. Transmitting LoRa at 22 dBm into the wrong antenna, or none, can damage the SX1262.
- **Cables.** Route the pigtails apart inside the enclosure and keep them short. Don't bundle the GNSS cable with a transmitter's.
- **Grant band.** The Wio-SX1262's FCC modular grant (FCC ID Z4T-WIO-SX1262) covers 902.3-914.9 MHz. Keep experiments inside that range.

## First-board test

Watch satellite C/N0 in u-center while LoRa transmits continuously, then again with WiFi on. A drop of a few dB is fine. If fixes drop out, gate GPS reads around transmissions or move the antennas farther apart.
