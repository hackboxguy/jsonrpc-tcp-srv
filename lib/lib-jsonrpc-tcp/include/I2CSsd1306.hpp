#ifndef __I2C_SSD_1306_H_
#define __I2C_SSD_1306_H_
#define DISPLAY_I2C_ADDR (0x3C)
#define WIDTH 128
#define HEIGHT 8
#define TILE_SIZE 8
#define STEP_SIZE 16
#include "DisplayDevice.hpp"
#include "ss_oled.h"
#include <stdint.h>
class I2CSsd1306 : public DisplayDevice {
  SSOLED ssoled;
  unsigned char ucBackBuf[1024];
  bool ssoledinit;
  std::string devNode, devType;
  uint8_t *screenBuf_;
  uint8_t *data_;
  uint8_t *tmp_;
  void clearBuffer();
  void clearScreen();
  bool writeScreen();
  bool writeCenter(std::string text, uint8_t row);
  bool writeText(std::string text, uint8_t row, uint8_t col);
  bool writeCommand(uint8_t bytes, uint8_t byte1);
  bool writeCommand(uint8_t bytes, uint8_t byte1, uint8_t byte2);
  bool writeCommand(uint8_t bytes, uint8_t byte1, uint8_t byte2, uint8_t byte3);
  uint8_t reverseByte(uint8_t b);
  bool writeImage(uint8_t *image);
  void getTileFromBitmap(int index);
  void setTileInBuffer(int index);
  void rotateTile();

public:
  I2CSsd1306(std::string DevNode, std::string DevType);
  ~I2CSsd1306();
  RPC_SRV_RESULT init_display();
  RPC_SRV_RESULT clear_display();
  RPC_SRV_RESULT print_line(char *msg, DISPLAY_LINE line, TEXT_ALIGNMENT align);
  RPC_SRV_RESULT set_back_light(bool sts);
  RPC_SRV_RESULT get_back_light(bool &sts);
  RPC_SRV_RESULT init_display_new();
};
#endif
