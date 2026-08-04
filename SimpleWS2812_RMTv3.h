#ifndef SIMPLEWS2812_RMTV3_H
#define SIMPLEWS2812_RMTV3_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

enum SimpleColor {BLACK = 0,RED,ORANGE,YELLOW,LIMA,GREEN,VERDAZUL,CYAN,AZUL,BLUE,PURPLE,MAGENTA,ROJIZO,WHITE};

template <uint16_t N>
class SimpleWS2812_RMTv3 {
public:
  explicit SimpleWS2812_RMTv3(uint8_t pin)
  : _strip(N, pin, NEO_GRB + NEO_KHZ800), _brightness(255) {}

  bool begin() {
    _strip.begin();
    _strip.setBrightness(_brightness);
    _strip.clear();
    _strip.show();
    return true;
  }

  void clear() {
    _strip.clear();
  }

  void setBrightness(uint8_t brightness) {
    _brightness = brightness;
    _strip.setBrightness(brightness);
  }

  void setRGB(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= N) return;
    _strip.setPixelColor(index, _strip.Color(r, g, b));
  }

  void setColor(uint16_t index, SimpleColor color) {
    if (index >= N) return;

    uint8_t r = 0, g = 0, b = 0;
    switch (color) {
      case RED:     r = 255; break;
      case GREEN:   g = 255; break;
      case BLUE:    b = 255; break;
      case ORANGE:  r = 255; g = 128; break;
      case YELLOW:  r = 255; g = 255; break;
      case LIMA:    g = 255; r = 128; break;
      case VERDAZUL:g = 255; b = 128; break;
      case CYAN:    g = 255; b = 255; break;
      case AZUL:    g = 128; b = 255; break;
      case PURPLE:  b = 255; r = 128; break;
      case MAGENTA: r = 255; b = 255; break;
      case ROJIZO:  r = 255; b = 128; break;
      case WHITE:   r = 255; g = 255; b = 255; break;
      case BLACK:
      default: break;
    }
    setRGB(index, r, g, b);
  }

  bool show() {
    _strip.show();
    return true;
  }

private:
  Adafruit_NeoPixel _strip;
  uint8_t _brightness;
};

#endif