# Firmware notes

Pin assignments are in [`firmware/include/pins.h`](../firmware/include/pins.h).

## ESP32-S3 module

- ESP32-S3-WROOM-1U-N8R8: enable PSRAM in octal mode (Arduino: "OPI PSRAM"; ESP-IDF: Octal SPI RAM). Put large buffers there, such as ML tensor arenas and display frame buffers. Keep small, time-critical buffers in internal RAM.
- Console is on native USB (USB-Serial-JTAG). If the USB port is ever used in device mode (mass storage, MIDI), the console moves to UART0 on the debug header (GPIO43/44).

## GNSS (MAX-M10S)

- UART1 at **38400 baud**, the MAX-M10S default (not 9600 as on older u-blox modules).
- PPS on GPIO16 for timestamping IMU samples.
- RESET_N is not connected. Reset the module over UART (UBX-CFG-RST) if needed.
- SAFEBOOT is on a test point for firmware recovery.
- A CR2032 on V_BCKP keeps ephemeris with the board switched off, for fast fixes at power-up.

## LoRa (Wio-SX1262)

Use RadioLib. Configure it the way Meshtastic configures the Wio-SX1262 on the XIAO ESP32S3 kit:

- DIO2 as the RF switch (TX path inside the module).
- RF_SW (GPIO10) driven as RXEN; TXEN not connected.
- TCXO on DIO3 at 1.8 V (confirm against the Wio-SX1262 documentation).
- Reset is tied to EN, so pass `RADIOLIB_NC` as the reset pin. A software restart doesn't reset the radio; RadioLib reinitialises it on boot.
- The IMU (SPI mode 3) and the SX1262 (mode 0) share a bus. Both libraries set their own SPI settings per transaction. Never assert both chip selects at once.

With GPS, a display and the SX1262, the board is also a complete Meshtastic node, though the ST7789 needs a custom variant file.

## IMU (BNO085)

- SPI mode, set in hardware: PS1 high, PS0 (WAKE, GPIO47) pulled up so it reads high at reset.
- INT on GPIO21 can wake the S3 from deep sleep (for example on significant motion).
- The BNO085 has a built-in activity classifier, which is a useful baseline for any custom model.

## Barometer (BMP390)

- I2C address 0x76 (SDO to GND). INT is not connected; poll it.

## Fuel gauge (MAX17048)

- I2C address 0x36. Gives state of charge and cell voltage. ALRT is not connected.

## Display (ST7789, 240 x 320)

- SPI3 on its own pins: SCK GPIO6, MOSI GPIO46, DC GPIO45, backlight PWM GPIO38.
- CS is tied to GND and RES to EN, so pass -1 for both in TFT_eSPI, LovyanGFX or esp_lcd.
- Start at 40 MHz SPI; drop to 26 MHz if edges ring.
- Most IPS ST7789 panels need colour inversion enabled.
- Send a software reset in init as well, since a software restart doesn't pulse EN.
- The backlight draws roughly 40-80 mA. Dim it with PWM and turn it off when idle.

## microSD

- SDMMC 4-bit (CLK 39, CMD 40, D0 41, D1 42, D2 2, D3 1).
- No card-detect pin. Detect a missing card from a failed mount or write.
- Flush or close the log file often, so pulling the card loses seconds of data rather than the file.

## Buttons

- User buttons on GPIO4 and GPIO5, active low with 10 k pull-ups. Both can wake the S3 from deep sleep.
- They're also handy for labelling data during recording (press at the start of an activity).

## Radio scheduling

Don't let WiFi and LoRa transmit at the same time (see [antennas.md](antennas.md)). It protects GNSS reception and saves battery.

## Edge ML ideas

The N8R8's 8 MB PSRAM leaves room for small models. Candidates:

- Activity classification from IMU, GPS speed and pressure (walking, cycling, driving, lifts), tagged into the log.
- Short "semantic" LoRa packets ("cycling, 14 km/h, climbing") instead of raw positions.
- Crash or fall detection with a position alert over LoRa.
- Vibration anomaly detection on bikes, vehicles or shop machines (train on normal, flag drift).
- Gesture recognition, possibly as a BLE MIDI controller.
- Learning when the device is stationary, to power the GNSS down.
- Weather-front prediction from the pressure trend.

Toolchains: TensorFlow Lite Micro with Espressif's ESP-NN optimisations, or Edge Impulse (supports the S3 directly).
