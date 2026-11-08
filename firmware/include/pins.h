// Tracker (GPS Logger V3) pin assignments.
// Matches hardware/schematic/Tracker_Schematic.pdf (2026-10-24).
// Module: ESP32-S3-WROOM-1U-N8R8. GPIO35-37 are used by the octal PSRAM; do not use them.
#pragma once

// ---- Shared SPI bus: BNO085 IMU + Wio-SX1262 LoRa ----
#define SPI_MOSI      11
#define SPI_SCK       12
#define SPI_MISO      13

#define IMU_CS        14
#define IMU_INT       21   // active low, 10k pull-up, deep-sleep wake capable
#define IMU_WAKE      47   // BNO085 PS0; pulled up so the IMU boots in SPI mode
#define IMU_RST       48

#define LORA_NSS       3   // 4.7k pull-up
#define LORA_DIO1      8   // interrupt, deep-sleep wake capable
#define LORA_BUSY      9
#define LORA_RXEN     10   // RF_SW; DIO2 switches TX inside the module
#define LORA_RST      -1   // tied to EN; use RADIOLIB_NC in RadioLib

// ---- Display: ST7789 240x320 on SPI3 (own pins) ----
#define TFT_SCK        6
#define TFT_MOSI      46   // strapping pin; must read low at reset
#define TFT_DC        45   // strapping pin; must read low at reset (no pull-up allowed)
#define TFT_BL        38   // backlight PWM (LEDC)
#define TFT_CS        -1   // tied to GND
#define TFT_RST       -1   // tied to EN

// ---- GNSS: MAX-M10S on UART1 ----
#define GPS_BAUD   38400   // MAX-M10S default
#define GPS_TX_PIN     7   // ESP32 TX -> GPS RXD (schematic net GPS_RX)
#define GPS_RX_PIN    15   // ESP32 RX <- GPS TXD (schematic net GPS_TX)
#define GPS_PPS       16   // deep-sleep wake capable

// ---- I2C: BMP390 (0x76), MAX17048 (0x36) ----
#define I2C_SDA       18
#define I2C_SCL       17
#define BMP390_ADDR   0x76
#define MAX17048_ADDR 0x36

// ---- microSD, SDMMC 4-bit, no card detect ----
#define SD_CLK        39
#define SD_CMD        40
#define SD_D0         41
#define SD_D1         42
#define SD_D2          2
#define SD_D3          1

// ---- Buttons (active low, 10k pull-ups, deep-sleep wake capable) ----
#define BTN_1          4
#define BTN_2          5
#define BTN_BOOT       0   // strapping pin; usable as a button after boot

// ---- Debug UART0 (header) ----
#define DEBUG_TX      43
#define DEBUG_RX      44
