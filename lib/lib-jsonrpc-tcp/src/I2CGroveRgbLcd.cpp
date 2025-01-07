#include "I2CGroveRgbLcd.hpp"
#include <unistd.h>
const unsigned char color_define[4][3] = {
    {255, 255, 255},
    {255, 0, 0},
    {0, 255, 0},
    {0, 0, 255},
};
I2CGroveRgbLcd::I2CGroveRgbLcd(std::string DevNode) : I2CBusAccess(DevNode) {}
I2CGroveRgbLcd::~I2CGroveRgbLcd() {}
void I2CGroveRgbLcd::i2c_send_byte(unsigned char dta) {
  write_byte((uint32_t)LCD_ADDRESS, dta);
}
void I2CGroveRgbLcd::i2c_send_byteS(unsigned char *dta, unsigned char len) {}
inline void I2CGroveRgbLcd::command(uint8_t value) {
  unsigned char dta[2] = {0x80, value};
  i2c_send_byteS(dta, 2);
}
inline unsigned int I2CGroveRgbLcd::write(uint8_t value) {
  unsigned char dta[2] = {0x40, value};
  i2c_send_byteS(dta, 2);
  return 1;
}
void I2CGroveRgbLcd::setReg(unsigned char addr, unsigned char dta) {
  write_byte((uint32_t)RGB_ADDRESS, dta);
}
void I2CGroveRgbLcd::setRGB(unsigned char r, unsigned char g, unsigned char b) {
  setReg(REG_RED, r);
  setReg(REG_GREEN, g);
  setReg(REG_BLUE, b);
}
void I2CGroveRgbLcd::setColor(unsigned char color) {
  if (color > 3)
    return;
  setRGB(color_define[color][0], color_define[color][1],
         color_define[color][2]);
}
void I2CGroveRgbLcd::blinkLED(void) {
  setReg(0x07, 0x17);
  setReg(0x06, 0x7f);
}
void I2CGroveRgbLcd::noBlinkLED(void) {
  setReg(0x07, 0x00);
  setReg(0x06, 0xff);
}
void I2CGroveRgbLcd::noAutoscroll(void) {
  _displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}
void I2CGroveRgbLcd::createChar(uint8_t location, uint8_t charmap[]) {
  location &= 0x7;
  command(LCD_SETCGRAMADDR | (location << 3));
  unsigned char dta[9];
  dta[0] = 0x40;
  for (int i = 0; i < 8; i++) {
    dta[i + 1] = charmap[i];
  }
  i2c_send_byteS(dta, 9);
}
void I2CGroveRgbLcd::scrollDisplayRight(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}
void I2CGroveRgbLcd::leftToRight(void) {
  _displaymode |= LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}
void I2CGroveRgbLcd::rightToLeft(void) {
  _displaymode &= ~LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}
void I2CGroveRgbLcd::autoscroll(void) {
  _displaymode |= LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}
void I2CGroveRgbLcd::noCursor() {
  _displaycontrol &= ~LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void I2CGroveRgbLcd::cursor() {
  _displaycontrol |= LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void I2CGroveRgbLcd::noBlink() {
  _displaycontrol &= ~LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void I2CGroveRgbLcd::blink() {
  _displaycontrol |= LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void I2CGroveRgbLcd::scrollDisplayLeft(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void I2CGroveRgbLcd::clear() {
  command(LCD_CLEARDISPLAY);
  usleep(2000);
}
void I2CGroveRgbLcd::home() {
  command(LCD_RETURNHOME);
  usleep(2000);
}
void I2CGroveRgbLcd::setCursor(uint8_t col, uint8_t row) {
  col = (row == 0 ? col | 0x80 : col | 0xc0);
  unsigned char dta[2] = {0x80, col};
  i2c_send_byteS(dta, 2);
}
void I2CGroveRgbLcd::noDisplay() {
  _displaycontrol &= ~LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void I2CGroveRgbLcd::display() {
  _displaycontrol |= LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void I2CGroveRgbLcd::begin(uint8_t cols, uint8_t lines, uint8_t dotsize) {
  if (lines > 1) {
    _displayfunction |= LCD_2LINE;
  }
  _numlines = lines;
  _currline = 0;
  if ((dotsize != 0) && (lines == 1)) {
    _displayfunction |= LCD_5x10DOTS;
  }
  usleep(50000);
  command(LCD_FUNCTIONSET | _displayfunction);
  usleep(4500);
  command(LCD_FUNCTIONSET | _displayfunction);
  usleep(150);
  command(LCD_FUNCTIONSET | _displayfunction);
  command(LCD_FUNCTIONSET | _displayfunction);
  _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
  display();
  clear();
  _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
  setReg(REG_MODE1, 0);
  setReg(REG_OUTPUT, 0xFF);
  setReg(REG_MODE2, 0x20);
  setColorWhite();
}
