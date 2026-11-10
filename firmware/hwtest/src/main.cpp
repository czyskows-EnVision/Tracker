// Tracker hardware bring-up test.
//
// On boot it runs one test per subsystem, prints a PASS/FAIL report over USB
// serial, shows the summary on the display, then switches to a live view of
// GPS, battery, pressure, IMU, LoRa and buttons.
//
// Serial commands (115200, any terminal):
//   h  help            r  rerun all tests
//   t  send one LoRa packet (antenna must be connected)
//   g  toggle raw NMEA passthrough
//   b  backlight sweep  s  write a line to the SD card
//
// Pins come from firmware/include/pins.h.

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <FS.h>
#include <SD_MMC.h>
#include <RadioLib.h>
#include <Adafruit_BNO08x.h>
#include <Adafruit_BMP3XX.h>
#include <TinyGPSPlus.h>
#include "pins.h"
#include "display.h"

// ---------------------------------------------------------------- settings

// LoRa bench settings. 906.875 MHz is Meshtastic's US default slot and sits
// inside the Wio-SX1262 FCC grant (902.3-914.9 MHz). 10 dBm is plenty on a bench.
static const float LORA_FREQ_MHZ = 906.875f;
static const float LORA_BW_KHZ = 125.0f;
static const uint8_t LORA_SF = 9;
static const uint8_t LORA_CR = 7;
static const int8_t LORA_POWER_DBM = 10;
static const float LORA_TCXO_V = 1.8f;   // Wio-SX1262 TCXO on DIO3 (as configured by Meshtastic)

static const uint32_t PSRAM_EXPECTED = 8u * 1024 * 1024;
static const uint32_t FLASH_EXPECTED = 8u * 1024 * 1024;

// ---------------------------------------------------------------- devices

TrackerDisplay tft;
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, RADIOLIB_NC, LORA_BUSY, SPI);
Adafruit_BNO08x bno(IMU_RST);
Adafruit_BMP3XX bmp;
TinyGPSPlus gps;

// ---------------------------------------------------------------- results

enum Status : uint8_t { PASS, FAIL, WARN, LOOK };   // LOOK = needs a visual check

struct Result {
  const char *name;
  Status status;
  char detail[80];
};

static Result results[16];
static int resultCount = 0;

static void report(const char *name, Status s, const char *fmt, ...) {
  if (resultCount >= (int)(sizeof(results) / sizeof(results[0]))) return;
  Result &r = results[resultCount++];
  r.name = name;
  r.status = s;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(r.detail, sizeof(r.detail), fmt, ap);
  va_end(ap);
  static const char *tag[] = {"PASS", "FAIL", "WARN", "LOOK"};
  Serial.printf("[%s] %-10s %s\n", tag[s], name, r.detail);
}

// ---------------------------------------------------------------- state

static bool okDisplay = false, okImu = false, okBmp = false, okGauge = false;
static bool okRadio = false, okSd = false;
static uint32_t gpsBaud = 0;

static volatile uint32_t ppsCount = 0;
static volatile bool loraRx = false;
static bool nmeaPassthrough = false;
static uint32_t loraTxCount = 0;
static float lastRssi = NAN, lastSnr = NAN;
static char lastRxText[40] = "";
static float quatI, quatJ, quatK, quatR;
static uint32_t imuEvents = 0;

void IRAM_ATTR onPps() { ppsCount++; }
void IRAM_ATTR onLoraPacket() { loraRx = true; }

// ---------------------------------------------------------------- helpers

static bool i2cProbe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool readReg16(uint8_t addr, uint8_t reg, uint16_t &out) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, 2) != 2) return false;
  out = ((uint16_t)Wire.read() << 8) | Wire.read();
  return true;
}

// MAX17048 registers (big-endian 16-bit)
static bool gaugeRead(float &volts, float &percent, uint16_t &version) {
  uint16_t v, s;
  if (!readReg16(MAX17048_ADDR, 0x08, version)) return false;
  if (!readReg16(MAX17048_ADDR, 0x02, v)) return false;
  if (!readReg16(MAX17048_ADDR, 0x04, s)) return false;
  volts = v * 78.125e-6f;   // 78.125 uV per LSB
  percent = s / 256.0f;     // 1/256 % per LSB
  return true;
}

// Hold every SPI chip select high before anything touches the shared bus.
static void deselectAll() {
  pinMode(IMU_CS, OUTPUT);
  digitalWrite(IMU_CS, HIGH);
  pinMode(LORA_NSS, OUTPUT);
  digitalWrite(LORA_NSS, HIGH);
}

// ---------------------------------------------------------------- tests

static void testChip() {
  uint32_t psram = ESP.getPsramSize();
  uint32_t flash = ESP.getFlashChipSize();
  Serial.printf("           %s rev %d, %d cores, %lu MHz\n", ESP.getChipModel(),
                ESP.getChipRevision(), ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz());

  report("Flash", flash == FLASH_EXPECTED ? PASS : WARN, "%lu MB (expect 8)",
         (unsigned long)(flash >> 20));

  if (psram < PSRAM_EXPECTED - 512 * 1024) {
    report("PSRAM", FAIL, "%lu KB found, expect 8 MB. Check OPI PSRAM setting",
           (unsigned long)(psram >> 10));
    return;
  }
  // Write and verify 4 MB of PSRAM.
  const size_t n = 4u * 1024 * 1024 / 4;
  uint32_t *buf = (uint32_t *)ps_malloc(n * 4);
  if (!buf) {
    report("PSRAM", FAIL, "ps_malloc(4 MB) failed");
    return;
  }
  for (size_t i = 0; i < n; i++) buf[i] = (uint32_t)(i * 2654435761u);
  size_t bad = 0;
  for (size_t i = 0; i < n; i++)
    if (buf[i] != (uint32_t)(i * 2654435761u)) bad++;
  free(buf);
  report("PSRAM", bad ? FAIL : PASS, "%lu MB, 4 MB pattern test %s", (unsigned long)(psram >> 20),
         bad ? "had errors" : "OK");
}

static void testDisplay() {
  okDisplay = tft.init();
  if (!okDisplay) {
    report("Display", FAIL, "init failed");
    return;
  }
  tft.setRotation(0);
  tft.setBrightness(200);
  // Colour bars: red, green, blue, white, black from top to bottom.
  const uint16_t colours[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
  for (int i = 0; i < 5; i++) tft.fillRect(0, i * 64, 240, 64, colours[i]);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(8, 4 * 64 + 22);
  tft.print("Tracker hwtest");
  delay(1500);
  report("Display", LOOK, "bars should read red, green, blue, white, black");
}

static void testI2c() {
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  char found[60] = "";
  int count = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    if (i2cProbe(a)) {
      char s[6];
      snprintf(s, sizeof(s), "%s0x%02X", count ? " " : "", a);
      strncat(found, s, sizeof(found) - strlen(found) - 1);
      count++;
    }
  }
  bool gauge = i2cProbe(MAX17048_ADDR), baro = i2cProbe(BMP390_ADDR);
  report("I2C bus", (gauge && baro) ? PASS : FAIL, "found %s%s", count ? found : "nothing",
         (gauge && baro) ? "" : " (expect 0x36 and 0x76)");
}

static void testGauge() {
  float v, pct;
  uint16_t ver;
  okGauge = gaugeRead(v, pct, ver);
  if (!okGauge) {
    report("Gauge", FAIL, "no reply at 0x36");
    return;
  }
  // With no battery the gauge is unpowered (it runs from the cell), so it won't answer at all.
  Status s = (v > 2.5f && v < 4.35f) ? PASS : WARN;
  report("Gauge", s, "%.3f V, %.1f %%, version 0x%04X", v, pct, ver);
}

static void testBaro() {
  okBmp = bmp.begin_I2C(BMP390_ADDR, &Wire);
  if (!okBmp) {
    report("Barometer", FAIL, "BMP390 not found at 0x76");
    return;
  }
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);
  bmp.performReading();   // first reading after config is often stale
  delay(50);
  if (!bmp.performReading()) {
    okBmp = false;
    report("Barometer", FAIL, "chip ID 0x%02X but reading failed", bmp.chipID());
    return;
  }
  float hpa = bmp.pressure / 100.0f;
  Status s = (hpa > 850 && hpa < 1100 && bmp.temperature > -10 && bmp.temperature < 60) ? PASS : WARN;
  report("Barometer", s, "%.2f hPa, %.1f C, chip ID 0x%02X", hpa, bmp.temperature, bmp.chipID());
}

static void testImu() {
  // PS0/WAKE must be high at reset for SPI mode (pulled up on the board too).
  pinMode(IMU_WAKE, OUTPUT);
  digitalWrite(IMU_WAKE, HIGH);
  okImu = bno.begin_SPI(IMU_CS, IMU_INT, &SPI);
  if (!okImu) {
    report("IMU", FAIL, "BNO085 did not answer over SPI");
    return;
  }
  if (!bno.enableReport(SH2_GAME_ROTATION_VECTOR, 20000)) {
    report("IMU", WARN, "found, but enabling the rotation report failed");
    return;
  }
  // Wait up to 1 s for a sensor event.
  sh2_SensorValue_t ev;
  uint32_t t0 = millis();
  bool got = false;
  while (millis() - t0 < 1000 && !got) got = bno.getSensorEvent(&ev);
  const sh2_ProductId_t &p = bno.prodIds.entry[0];
  report("IMU", got ? PASS : WARN, "BNO085 fw %u.%u.%u, part %lu%s", p.swVersionMajor,
         p.swVersionMinor, p.swVersionPatch, (unsigned long)p.swPartNumber,
         got ? ", rotation events OK" : ", no events in 1 s");
}

static void testRadio() {
  int16_t st = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                           RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_POWER_DBM, 8, LORA_TCXO_V, false);
  if (st != RADIOLIB_ERR_NONE) {
    okRadio = false;
    report("LoRa", FAIL, "begin() error %d (-2: no reply on SPI, -707: TCXO/clock)", st);
    return;
  }
  radio.setDio2AsRfSwitch(true);                  // DIO2 switches TX inside the module
  radio.setRfSwitchPins(LORA_RXEN, RADIOLIB_NC);  // RF_SW driven as RXEN
  radio.setPacketReceivedAction(onLoraPacket);
  st = radio.startReceive();
  okRadio = (st == RADIOLIB_ERR_NONE);
  report("LoRa", okRadio ? PASS : FAIL, "SX1262 at %.3f MHz, SF%u, listening%s", LORA_FREQ_MHZ,
         LORA_SF, okRadio ? "" : " failed");
}

static uint32_t countNmea(uint32_t baud, uint32_t ms) {
  Serial1.end();
  Serial1.begin(baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  uint32_t before = gps.passedChecksum();
  uint32_t t0 = millis();
  while (millis() - t0 < ms)
    while (Serial1.available()) gps.encode(Serial1.read());
  return gps.passedChecksum() - before;
}

static void testGps() {
  pinMode(GPS_PPS, INPUT);
  attachInterrupt(digitalPinToInterrupt(GPS_PPS), onPps, RISING);
  uint32_t n = countNmea(GPS_BAUD, 2500);
  gpsBaud = GPS_BAUD;
  if (n == 0) {
    // Older u-blox defaults, or a module that was reconfigured.
    n = countNmea(9600, 2500);
    if (n) gpsBaud = 9600;
  }
  if (n == 0) {
    gpsBaud = GPS_BAUD;
    Serial1.end();
    Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    report("GNSS", FAIL, "no NMEA at 38400 or 9600 (chars seen: %lu). Check TX/RX swap",
           (unsigned long)gps.charsProcessed());
    return;
  }
  report("GNSS", gpsBaud == GPS_BAUD ? PASS : WARN, "%lu good sentences at %lu baud, %s", (unsigned long)n,
         (unsigned long)gpsBaud,
         gps.satellites.isValid() ? "satellites reported" : "no fix yet (normal indoors)");
}

static void testSd() {
  SD_MMC.end();
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3);
  bool fourBit = SD_MMC.begin("/sdcard", false);
  if (!fourBit) {
    // 1-bit mode only uses CLK, CMD and D0; if this works, suspect D1-D3.
    SD_MMC.end();
    SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
    if (SD_MMC.begin("/sdcard", true)) {
      okSd = true;
      report("SD card", WARN, "mounts in 1-bit mode only. Check D1, D2, D3 (GPIO42, 2, 1)");
    } else {
      okSd = false;
      report("SD card", FAIL, "no card, or CLK/CMD/D0 problem (GPIO39, 40, 41)");
      return;
    }
  } else {
    okSd = true;
  }
  File f = SD_MMC.open("/hwtest.txt", FILE_WRITE);
  bool wrote = f && f.println("Tracker hwtest write check");
  if (f) f.close();
  f = SD_MMC.open("/hwtest.txt", FILE_READ);
  String back = f ? f.readStringUntil('\n') : String();
  if (f) f.close();
  bool readOk = back.startsWith("Tracker hwtest");
  uint32_t mb = (uint32_t)(SD_MMC.cardSize() / (1024 * 1024));
  if (fourBit)
    report("SD card", (wrote && readOk) ? PASS : FAIL, "%lu MB, 4-bit, write/read %s",
           (unsigned long)mb, (wrote && readOk) ? "OK" : "failed");
}

static void testButtons() {
  const int pins[] = {BTN_1, BTN_2, BTN_BOOT};
  const char *names[] = {"B1", "B2", "BOOT"};
  char held[32] = "";
  for (int i = 0; i < 3; i++) {
    pinMode(pins[i], INPUT);   // external 10k pull-ups
    if (digitalRead(pins[i]) == LOW) {
      strncat(held, names[i], sizeof(held) - strlen(held) - 1);
      strncat(held, " ", sizeof(held) - strlen(held) - 1);
    }
  }
  if (held[0])
    report("Buttons", FAIL, "reading pressed at boot: %s(held, or shorted to GND)", held);
  else
    report("Buttons", PASS, "all released; press each one in the live view");
}

// ---------------------------------------------------------------- screens

static void drawSummary() {
  if (!okDisplay) return;
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(6, 6);
  tft.print("Bring-up results");
  int y = 34;
  for (int i = 0; i < resultCount; i++) {
    const Result &r = results[i];
    uint16_t c = r.status == PASS ? TFT_GREEN : r.status == FAIL ? TFT_RED
               : r.status == WARN ? TFT_YELLOW : TFT_CYAN;
    static const char *tag[] = {"PASS", "FAIL", "WARN", "LOOK"};
    tft.setTextColor(c, TFT_BLACK);
    tft.setCursor(6, y);
    tft.print(tag[r.status]);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(66, y);
    tft.print(r.name);
    y += 22;
  }
}

static void runAll() {
  resultCount = 0;
  Serial.println();
  Serial.println("==================== Tracker bring-up ====================");
  testChip();
  testDisplay();
  testButtons();
  testI2c();
  testGauge();
  testBaro();
  testImu();
  testRadio();
  testGps();
  testSd();

  int fails = 0, warns = 0;
  for (int i = 0; i < resultCount; i++) {
    if (results[i].status == FAIL) fails++;
    if (results[i].status == WARN) warns++;
  }
  Serial.printf("---- %d tests: %d failed, %d warnings. Type h for commands.\n", resultCount,
                fails, warns);
  drawSummary();
  delay(4000);
  if (okDisplay) tft.fillScreen(TFT_BLACK);
}

static void drawLive() {
  if (!okDisplay) return;
  char line[40];
  int y = 6;
  auto row = [&](const char *label, const char *value, uint16_t colour) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(6, y);
    tft.print(label);
    tft.setTextColor(colour, TFT_BLACK);
    tft.setCursor(70, y);
    tft.printf("%-14s", value);
    y += 26;
  };
  tft.setTextSize(2);

  snprintf(line, sizeof(line), "%lu sats %s", gps.satellites.isValid() ? (unsigned long)gps.satellites.value() : 0UL,
           gps.location.isValid() ? "FIX" : "");
  row("GPS", line, gps.location.isValid() ? TFT_GREEN : TFT_YELLOW);
  if (gps.location.isValid())
    snprintf(line, sizeof(line), "%.5f", gps.location.lat());
  else
    snprintf(line, sizeof(line), "--");
  row(" lat", line, TFT_WHITE);
  if (gps.location.isValid())
    snprintf(line, sizeof(line), "%.5f", gps.location.lng());
  else
    snprintf(line, sizeof(line), "--");
  row(" lon", line, TFT_WHITE);
  snprintf(line, sizeof(line), "%lu", (unsigned long)ppsCount);
  row("PPS", line, ppsCount ? TFT_GREEN : TFT_LIGHTGREY);

  float v = 0, pct = 0;
  uint16_t ver;
  if (gaugeRead(v, pct, ver))
    snprintf(line, sizeof(line), "%.2fV %.0f%%", v, pct);
  else
    snprintf(line, sizeof(line), "no gauge");
  row("Batt", line, TFT_WHITE);

  if (okBmp && bmp.performReading())
    snprintf(line, sizeof(line), "%.1fhPa %.1fC", bmp.pressure / 100.0f, bmp.temperature);
  else
    snprintf(line, sizeof(line), "--");
  row("Baro", line, TFT_WHITE);

  if (okImu)
    snprintf(line, sizeof(line), "%+.2f %+.2f %+.2f", quatI, quatJ, quatK);
  else
    snprintf(line, sizeof(line), "--");
  row("IMU", line, TFT_WHITE);

  if (!isnan(lastRssi))
    snprintf(line, sizeof(line), "%.0fdBm %.1fdB", lastRssi, lastSnr);
  else
    snprintf(line, sizeof(line), okRadio ? "listening" : "radio off");
  row("LoRa", line, TFT_WHITE);

  snprintf(line, sizeof(line), "%s %s %s", digitalRead(BTN_1) ? "--" : "B1", digitalRead(BTN_2) ? "--" : "B2",
           digitalRead(BTN_BOOT) ? "----" : "BOOT");
  row("Btns", line, TFT_CYAN);

  snprintf(line, sizeof(line), "%s", okSd ? "mounted" : "none");
  row("SD", line, okSd ? TFT_GREEN : TFT_LIGHTGREY);
}

// ---------------------------------------------------------------- commands

static void help() {
  Serial.println("Commands: h help | r rerun tests | t send LoRa packet | g toggle NMEA passthrough |"
                 " b backlight sweep | s write line to SD");
}

static void loraTransmit() {
  if (!okRadio) {
    Serial.println("LoRa not initialised.");
    return;
  }
  char msg[48];
  snprintf(msg, sizeof(msg), "Tracker hwtest #%lu", (unsigned long)++loraTxCount);
  int16_t st = radio.transmit((uint8_t *)msg, strlen(msg));
  Serial.printf("LoRa TX \"%s\": %s (%d)\n", msg, st == RADIOLIB_ERR_NONE ? "sent" : "error", st);
  radio.startReceive();
}

static void backlightSweep() {
  if (!okDisplay) return;
  for (int b = 255; b >= 0; b -= 5) { tft.setBrightness(b); delay(8); }
  for (int b = 0; b <= 255; b += 5) { tft.setBrightness(b); delay(8); }
  tft.setBrightness(200);
  Serial.println("Backlight swept down and up.");
}

static void sdAppend() {
  if (!okSd) { Serial.println("No SD card mounted."); return; }
  File f = SD_MMC.open("/hwtest.txt", FILE_APPEND);
  if (!f) { Serial.println("Open for append failed."); return; }
  f.printf("t=%lu ms sats=%lu\n", (unsigned long)millis(),
           gps.satellites.isValid() ? (unsigned long)gps.satellites.value() : 0UL);
  f.close();
  Serial.println("Appended a line to /hwtest.txt.");
}

static void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'h': help(); break;
      case 'r': runAll(); break;
      case 't': loraTransmit(); break;
      case 'g':
        nmeaPassthrough = !nmeaPassthrough;
        Serial.printf("NMEA passthrough %s\n", nmeaPassthrough ? "on" : "off");
        break;
      case 'b': backlightSweep(); break;
      case 's': sdAppend(); break;
      default: break;
    }
  }
}

// ---------------------------------------------------------------- main

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);   // wait briefly for USB serial

  deselectAll();
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  pinMode(BTN_1, INPUT);
  pinMode(BTN_2, INPUT);
  pinMode(BTN_BOOT, INPUT);

  runAll();
  help();
}

void loop() {
  handleSerial();

  while (Serial1.available()) {
    char c = Serial1.read();
    gps.encode(c);
    if (nmeaPassthrough) Serial.write(c);
  }

  if (okImu) {
    if (bno.wasReset()) bno.enableReport(SH2_GAME_ROTATION_VECTOR, 20000);
    sh2_SensorValue_t ev;
    while (bno.getSensorEvent(&ev)) {
      if (ev.sensorId == SH2_GAME_ROTATION_VECTOR) {
        quatI = ev.un.gameRotationVector.i;
        quatJ = ev.un.gameRotationVector.j;
        quatK = ev.un.gameRotationVector.k;
        quatR = ev.un.gameRotationVector.real;
        imuEvents++;
      }
    }
  }

  if (loraRx) {
    loraRx = false;
    uint8_t buf[64];
    size_t len = radio.getPacketLength();
    if (len > sizeof(buf) - 1) len = sizeof(buf) - 1;
    int16_t st = radio.readData(buf, len);
    if (st == RADIOLIB_ERR_NONE) {
      buf[len] = 0;
      lastRssi = radio.getRSSI();
      lastSnr = radio.getSNR();
      strncpy(lastRxText, (char *)buf, sizeof(lastRxText) - 1);
      Serial.printf("LoRa RX \"%s\" RSSI %.0f dBm SNR %.1f dB\n", lastRxText, lastRssi, lastSnr);
    }
    radio.startReceive();
  }

  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 500) {
    lastDraw = millis();
    drawLive();
  }
}
