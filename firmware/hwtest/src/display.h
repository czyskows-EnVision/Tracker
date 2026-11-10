// LovyanGFX configuration for the 2.0" ST7789 240x320 IPS module.
// The display has its own SPI host (SPI3) so it never holds the IMU/LoRa bus.
// CS is tied to GND and RES to EN on the board.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "pins.h"

class TrackerDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

public:
  TrackerDisplay() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI3_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;   // drop to 26 MHz if the image is noisy
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = TFT_SCK;
      cfg.pin_mosi = TFT_MOSI;
      cfg.pin_miso = -1;           // write-only display
      cfg.pin_dc = TFT_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = -1;             // tied to GND
      cfg.pin_rst = -1;            // tied to EN; init sends a software reset
      cfg.pin_busy = -1;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;        // no MISO
      cfg.invert = true;           // most IPS ST7789 panels need inversion
      cfg.rgb_order = false;       // set true if red and blue come out swapped
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = TFT_BL;
      cfg.invert = false;
      cfg.freq = 12000;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};
