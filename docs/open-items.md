# Open items

## Changes made after the schematic PDF in this repo

The PDF in `hardware/schematic/` is the 2026-10-24 version. Since then:

- [ ] SAW filter changed to Qualcomm B39162B4327P810 (new footprint; same pinout: 1 in, 4 out, 2/3/5 GND).
- [ ] USBLC6 VBUS pin moved to the connector side of the input diode (net VBUS_IN).
- [ ] GNSS antenna connector is the MyAntenna A-SMA-KE-16.5A edge-mount SMA (LCSC C22467617). Confirm its clamp gap fits a 1.6 mm board.

Replace the PDF once these are in the schematic.

## Before ordering

- [ ] microSD socket part number. The footprint is "SD_Amphenol" with no part number.
- [ ] Fix library part descriptions that don't match their values: L2 (says 150 nH, should be 27 nH), the 0.1 µF 0603 caps (say 1 µF), the 0805 47 µF / 100 nF / 10 µF caps (say 22 µF), C21/C22 (say 1 µF), C17 (a 4 V part; use 6.3 V).
- [ ] Check the 4.2 x 3.2 mm switch footprint against the C&K PTS810 drawing.
- [ ] Measure a display module in hand: header position, mounting holes, and whether DC has a pull-up (it lands on strapping pin GPIO45).
- [ ] Check BT1 polarity on the footprint.
- [ ] Annotate any remaining unassigned designators.

## Bring-up

- [ ] Confirm both boot straps read correctly with the display fitted (board boots with the display module installed).
- [ ] GNSS C/N0 check with LoRa and WiFi transmitting (see [antennas.md](antennas.md)).
- [ ] Confirm the Wio-SX1262 TCXO voltage before configuring RadioLib.
- [ ] Write a hardware test firmware that exercises every peripheral against `firmware/include/pins.h`.

## Possible later changes

- Qwiic (JST SH 4-pin) connector on the I2C bus as the expansion port; every GPIO is in use.
- A 0 Ω option to bypass the SAW filter on prototypes.
