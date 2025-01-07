#include "I2CSsd1306.hpp"
#include "glcdfont.h"
#include <math.h>
#include <string.h>
#include <unistd.h>
I2CSsd1306::I2CSsd1306(std::string DevNode, std::string DevType)
    : DisplayDevice(DevNode) {
  devNode = DevNode;
  devType = DevType;
  screenBuf_ = NULL;
  data_ = NULL;
  tmp_ = NULL;
  ssoledinit = false;
  init_display();
}
I2CSsd1306::~I2CSsd1306() {
  if (ssoledinit)
    oledPower(&ssoled, 0);
  if (screenBuf_ != NULL) {
    delete[] screenBuf_;
    screenBuf_ = 0;
  }
}
void I2CSsd1306::clearBuffer() {
  if (screenBuf_)
    memset(screenBuf_, 0x00, (WIDTH * HEIGHT));
}
void I2CSsd1306::clearScreen() { oledFill(&ssoled, 0, 1); }
bool I2CSsd1306::writeScreen() {
  bool fOK = true;
  uint16_t uiIndex = 0x00;
  uint8_t uchTemp[0x09];
  uint8_t data[16];
  data[0] = 0x00;
  data[1] = 0x21;
  data[2] = 0x00;
  data[3] = 0x7F;
  write_array((uint32_t)DISPLAY_I2C_ADDR, data, 4);
  data[0] = 0x00;
  data[1] = 0x22;
  data[2] = 0x00;
  data[3] = 0x07;
  write_array((uint32_t)DISPLAY_I2C_ADDR, data, 4);
  while (uiIndex < (WIDTH * HEIGHT)) {
    uchTemp[0] = 0x40;
    for (int i = 0; i < 8; i++)
      uchTemp[i + 1] = screenBuf_[uiIndex + i];
    fOK = write_array((uint32_t)DISPLAY_I2C_ADDR, uchTemp, 0x09);
    uiIndex += 0x08;
  }
  return fOK;
}
bool I2CSsd1306::writeCenter(std::string text, uint8_t row) {
  int center = (128 - (text.length() * 8)) / 2;
  oledWriteString(&ssoled, 0, center, row, (char *)text.c_str(), FONT_NORMAL, 0,
                  1);
  return true;
  if ((text.length() * 6) < 127)
    return writeText(text, row, (128 - ((text.length() * 6) - 1)) / 2);
  else
    return writeText(text, row, 0);
}
bool I2CSsd1306::writeText(std::string text, uint8_t row, uint8_t col) {
  uint8_t uchI, uchJ;
  uint16_t uiIndex;
  uiIndex = (row * WIDTH) + col;
  if ((row >= HEIGHT) || (col >= WIDTH))
    return false;
  for (uchI = 0; uchI < text.length(); uchI++) {
    for (uchJ = 0; uchJ < 5; uchJ++) {
      screenBuf_[uiIndex] = font[(text[uchI] * 5) + uchJ];
      uiIndex++;
    }
    screenBuf_[uiIndex] = 0x00;
    uiIndex++;
  }
  return true;
}
RPC_SRV_RESULT I2CSsd1306::init_display() {
  return init_display_new();
  bool retval = false;
  screenBuf_ = new uint8_t[WIDTH * HEIGHT];
  memset(screenBuf_, 0, (WIDTH * HEIGHT));
  if (!screenBuf_) {
    return RPC_SRV_RESULT_MEM_ERROR;
  }
  data_ = new uint8_t[TILE_SIZE];
  memset(data_, 0, TILE_SIZE);
  if (!data_) {
    return RPC_SRV_RESULT_MEM_ERROR;
  }
  tmp_ = new uint8_t[TILE_SIZE];
  memset(tmp_, 0, TILE_SIZE);
  if (!tmp_) {
    return RPC_SRV_RESULT_MEM_ERROR;
  }
  retval &= writeCommand(1, 0xAE);
  retval &= writeCommand(2, 0xD5, 0x80);
  retval &= writeCommand(2, 0xA8, 0x3F);
  retval &= writeCommand(2, 0xD3, 0x00);
  retval &= writeCommand(1, 0x40);
  retval &= writeCommand(2, 0x8D, 0x14);
  retval &= writeCommand(2, 0x20, 0x00);
  retval &= writeCommand(1, 0xA1);
  retval &= writeCommand(1, 0xC8);
  retval &= writeCommand(2, 0xDA, 0x12);
  retval &= writeCommand(2, 0x81, 0xCF);
  retval &= writeCommand(2, 0xD9, 0xF1);
  retval &= writeCommand(2, 0xDB, 0x40);
  retval &= writeCommand(1, 0xA4);
  retval &= writeCommand(1, 0xA6);
  retval &= writeCommand(1, 0x00);
  retval &= writeCommand(1, 0x10);
  retval &= writeCommand(1, 0x40);
  retval &= writeCommand(1, 0xAF);
  retval &= writeCommand(3, 0x21, 0x00, 0x7F);
  retval &= writeCommand(3, 0x22, 0x00, 0x07);
  retval &= writeCommand(1, 0xAF);
  return RPC_SRV_RESULT_SUCCESS;
}
bool I2CSsd1306::writeCommand(uint8_t bytes, uint8_t byte1) {
  uint8_t data[16];
  data[0] = 0x00;
  data[1] = byte1;
  write_array((uint32_t)DISPLAY_I2C_ADDR, data, 2);
  return true;
}
bool I2CSsd1306::writeCommand(uint8_t bytes, uint8_t byte1, uint8_t byte2) {
  uint8_t data[16];
  data[0] = 0x00;
  data[1] = byte1;
  data[2] = byte2;
  write_array((uint32_t)DISPLAY_I2C_ADDR, data, 3);
  return true;
}
bool I2CSsd1306::writeCommand(uint8_t bytes, uint8_t byte1, uint8_t byte2,
                              uint8_t byte3) {
  uint8_t data[16];
  data[0] = 0x00;
  data[1] = byte1;
  data[2] = byte2;
  data[3] = byte3;
  write_array((uint32_t)DISPLAY_I2C_ADDR, data, 4);
  return true;
}
uint8_t I2CSsd1306::reverseByte(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}
bool I2CSsd1306::writeImage(uint8_t *image) {
  for (int lcv = 0; lcv < (WIDTH * HEIGHT); lcv++) {
    getTileFromBitmap(lcv);
    rotateTile();
    for (int i = 0; i < TILE_SIZE; i++) {
      tmp_[i] = reverseByte(tmp_[i]);
    }
    setTileInBuffer(lcv);
  }
  return false;
}
void I2CSsd1306::getTileFromBitmap(int index) {
  int step = 0;
  for (int i = 0; i < TILE_SIZE; i++, step += STEP_SIZE) {
    data_[i] = screenBuf_[index + step];
  }
}
void I2CSsd1306::rotateTile() {
  for (int i = 0; i < TILE_SIZE; i++) {
    for (int j = 0; j < TILE_SIZE; j++) {
      uint8_t temp = data_[j] & (uint8_t)pow(2, 7 - i);
      int shift = 7 - i - j;
      uint8_t shifted = 0x0;
      if (shift < 0) {
        shift *= -1;
        shifted = temp << shift;
      } else {
        shifted = temp >> shift;
      }
      tmp_[i] |= shifted;
    }
    tmp_[i] = reverseByte(tmp_[i]);
  }
}
void I2CSsd1306::setTileInBuffer(int index) {
  int step = 0;
  for (int i = 0; i < TILE_SIZE; i++, step += STEP_SIZE) {
    screenBuf_[index + step] |= tmp_[i];
  }
}
RPC_SRV_RESULT I2CSsd1306::clear_display() {
  clearScreen();
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT I2CSsd1306::print_line(char *msg, DISPLAY_LINE line,
                                      TEXT_ALIGNMENT align) {
  std::string str = msg;
  switch (line) {
  case DISPLAY_LINE_1:
    writeCenter(str, 0);
    break;
  case DISPLAY_LINE_2:
    writeCenter(str, 1);
    break;
  case DISPLAY_LINE_3:
    writeCenter(str, 2);
    break;
  case DISPLAY_LINE_4:
    writeCenter(str, 3);
    break;
  case DISPLAY_LINE_5:
    writeCenter(str, 4);
    break;
  case DISPLAY_LINE_6:
    writeCenter(str, 5);
    break;
  case DISPLAY_LINE_7:
    writeCenter(str, 6);
    break;
  case DISPLAY_LINE_8:
    writeCenter(str, 7);
    break;
  default:
    return RPC_SRV_RESULT_ARG_ERROR;
    break;
  }
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT I2CSsd1306::set_back_light(bool sts) {
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT I2CSsd1306::get_back_light(bool &sts) {
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT I2CSsd1306::init_display_new() {
  if (ssoledinit)
    return RPC_SRV_RESULT_SUCCESS;
  const char *table[] = ADLIB_DISPLAY_TYPE_TABL;
  int i = -1, iChannel = 0;
  int iOLEDAddr = -1;
  int iOLEDType = OLED_128x32;
  int bFlip = 1, bInvert = 0;
  if (strcmp(devType.c_str(), table[ADLIB_DISPLAY_TYPE_SSD1306_128x32]) == 0)
    iOLEDType = OLED_128x32;
  else if (strcmp(devType.c_str(), table[ADLIB_DISPLAY_TYPE_SSD1306_128x64]) ==
           0)
    iOLEDType = OLED_128x64;
  else
    iOLEDType = OLED_128x32;
  std::string node(devNode);
  int position = node.find("i2c-");
  std::string value = node.substr(position + 4);
  iChannel = std::stoi(value);
  i = oledInit(&ssoled, iOLEDType, iOLEDAddr, bFlip, bInvert, 1, iChannel,
               iOLEDAddr, -1, 400000);
  if (i != OLED_NOT_FOUND) {
    oledSetBackBuffer(&ssoled, ucBackBuf);
    oledFill(&ssoled, 0, 1);
    ssoledinit = true;
    return RPC_SRV_RESULT_SUCCESS;
  } else
    return RPC_SRV_RESULT_FAIL;
}
