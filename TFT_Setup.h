//  Configuracion sugerida en libreria (ESP32-S3 N16R8)
#define ST7789_DRIVER
#define TFT_RGB_ORDER TFT_BGR
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#define TFT_CS   10
#define TFT_MOSI 11
#define TFT_SCLK 12
//#define TFT_MISO -1  // no hace falta
#define TFT_DC    13
#define TFT_RST   9
#define TFT_BL    14
#define TFT_BACKLIGHT_ON HIGH
#define TOUCH_CS -1 //evitar warning

#define LOAD_GFXFF

#define USE_HSPI_PORT
#define SPI_FREQUENCY  80000000 

//**********************************