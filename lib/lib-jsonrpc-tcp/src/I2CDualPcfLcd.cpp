#include "I2CDualPcfLcd.hpp"
#include <string.h>
#include <unistd.h>
I2CDualPcfLcd::I2CDualPcfLcd(std::string DevNode, std::string DevType)
    : DisplayDevice(DevNode) {
  io_ctrl_byte = 0xff;
  init_lcd();
  clear_display_internal(DISPLAY_LINE_FULL);
}
I2CDualPcfLcd::~I2CDualPcfLcd() {}
void I2CDualPcfLcd::update_io_ctrl_device() {
  write_byte((uint32_t)LCD_PCF_CTRL_ADDRESS, io_ctrl_byte);
}
void I2CDualPcfLcd::update_io_ctrl_cache(uint8_t location, uint8_t value) {
  if (value)
    io_ctrl_byte |= (0x01 << location);
  else
    io_ctrl_byte &= ~(0x01 << location);
}
void I2CDualPcfLcd::write_inst(uint8_t byte) {
  update_io_ctrl_cache(0, 0);
  update_io_ctrl_cache(1, 0);
  update_io_ctrl_device();
  write_byte((uint32_t)LCD_PCF_DATA_ADDRESS, byte);
  update_io_ctrl_cache(0, 0);
  update_io_ctrl_cache(1, 1);
  update_io_ctrl_device();
  update_io_ctrl_cache(0, 0);
  update_io_ctrl_cache(1, 0);
  update_io_ctrl_device();
}
void I2CDualPcfLcd::write_data(uint8_t byte) {
  update_io_ctrl_cache(0, 1);
  update_io_ctrl_cache(1, 0);
  update_io_ctrl_device();
  write_byte((uint32_t)LCD_PCF_DATA_ADDRESS, byte);
  update_io_ctrl_cache(0, 1);
  update_io_ctrl_cache(1, 1);
  update_io_ctrl_device();
  update_io_ctrl_cache(0, 1);
  update_io_ctrl_cache(1, 0);
  update_io_ctrl_device();
}
#define DISPLAY_TYPE 16
void I2CDualPcfLcd::go_to(uint8_t x) {
  if (x > DISPLAY_TYPE) {
    x -= DISPLAY_TYPE;
    x += 0x40;
  }
  x = x - 1;
  x |= 0x80;
  write_inst(x);
}
void I2CDualPcfLcd::go_to_centre(char *ptr, DISPLAY_LINE line) {
  if (line == DISPLAY_LINE_2)
    go_to(((DISPLAY_TYPE - strlen(ptr)) / 2) + 1 + DISPLAY_TYPE);
  else
    go_to(((DISPLAY_TYPE - strlen(ptr)) / 2) + 1);
}
void I2CDualPcfLcd::init_lcd(void) {
  io_ctrl_byte = 0xFF;
  write_byte((uint32_t)LCD_PCF_CTRL_ADDRESS, io_ctrl_byte);
  write_inst(0x38);
  write_inst(0x38);
  write_inst(0x30);
  write_inst(0x30);
  write_inst(0x38);
  write_inst(0x14);
  write_inst(0x0C);
  write_inst(0x06);
  write_inst(0x01);
}
void I2CDualPcfLcd::print_lcd(char *ptr) {
  int i = 1;
  while (*ptr) {
    write_data(*ptr++);
    i++;
    if (i > 16)
      break;
  }
}
void I2CDualPcfLcd::clear_display_internal(DISPLAY_LINE line) {
  switch (line) {
  case DISPLAY_LINE_1:
    go_to(1);
    print_lcd((char *)"                ");
    break;
  case DISPLAY_LINE_2:
    go_to(17);
    print_lcd((char *)"                ");
    break;
  case DISPLAY_LINE_FULL:
    write_inst(0x01);
    break;
  default:
    break;
  }
}
void I2CDualPcfLcd::print_center(DISPLAY_LINE line, char *string) {
  clear_display_internal(line);
  go_to_centre(string, line);
  print_lcd(string);
}
RPC_SRV_RESULT I2CDualPcfLcd::print_line(char *msg, DISPLAY_LINE line,
                                         TEXT_ALIGNMENT align) {
  switch (align) {
  case TEXT_ALIGNMENT_LEFT:
    print_lcd(msg);
    break;
  case TEXT_ALIGNMENT_RIGHT:
    print_lcd(msg);
    break;
  default:
    print_center(line, msg);
    break;
  }
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT I2CDualPcfLcd::set_back_light(bool sts) {
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT I2CDualPcfLcd::get_back_light(bool &sts) {
  return RPC_SRV_RESULT_SUCCESS;
}
