#ifndef __DISPLAY_DEVICE_H_
#define __DISPLAY_DEVICE_H_
#include "ADCommon.hpp"
#include "I2CBusAccess.h"
#include <iostream>
#include <stdint.h>
using namespace std;
typedef enum DISPLAY_LINE_T {
  DISPLAY_LINE_1,
  DISPLAY_LINE_2,
  DISPLAY_LINE_3,
  DISPLAY_LINE_4,
  DISPLAY_LINE_5,
  DISPLAY_LINE_6,
  DISPLAY_LINE_7,
  DISPLAY_LINE_8,
  DISPLAY_LINE_FULL,
  DISPLAY_LINE_UNKNOWN,
  DISPLAY_LINE_NONE
} DISPLAY_LINE;
typedef enum TEXT_ALIGNMENT_T {
  TEXT_ALIGNMENT_LEFT,
  TEXT_ALIGNMENT_RIGHT,
  TEXT_ALIGNMENT_CENTER,
  TEXT_ALIGNMENT_UNKNOWN,
  TEXT_ALIGNMENT_NONE
} TEXT_ALIGNMENT;
typedef enum ADLIB_DISPLAY_TYPE_T {
  ADLIB_DISPLAY_TYPE_SSD1306_128x32,
  ADLIB_DISPLAY_TYPE_SSD1306_128x64,
  ADLIB_DISPLAY_TYPE_SSD1306_128x32_PI,
  ADLIB_DISPLAY_TYPE_SSD1306_128x64_PI,
  ADLIB_DISPLAY_TYPE_1602_DUAL_PCF,
  ADLIB_DISPLAY_TYPE_1602_PCF,
  ADLIB_DISPLAY_TYPE_2002_PCF,
  ADLIB_DISPLAY_TYPE_2004_PCF,
  ADLIB_DISPLAY_TYPE_1604_PCF,
  ADLIB_DISPLAY_TYPE_UNKNOWN,
  ADLIB_DISPLAY_TYPE_NONE
} ADLIB_DISPLAY_TYPE;
#define ADLIB_DISPLAY_TYPE_TABL                                                \
  {                                                                            \
    "ssd1306_128x32", "ssd1306_128x64", "ssd1306_128x32_pi",                   \
        "ssd1306_128x64_pi", "1602_dual_pcf", "1602_pcf", "2002_pcf",          \
        "2004_pcf", "1604_pcf", "unknown", "none", "\0"                        \
  }
class DisplayDevice : public I2CBusAccess {
public:
  DisplayDevice(std::string devnode);
  virtual ~DisplayDevice();
  virtual RPC_SRV_RESULT init_display() = 0;
  virtual RPC_SRV_RESULT clear_display() = 0;
  virtual RPC_SRV_RESULT
  print_line(char *msg, DISPLAY_LINE line = DISPLAY_LINE_1,
             TEXT_ALIGNMENT align = TEXT_ALIGNMENT_CENTER) = 0;
  virtual RPC_SRV_RESULT test_function() {
    return RPC_SRV_RESULT_FEATURE_NOT_AVAILABLE;
  };
  virtual RPC_SRV_RESULT set_back_light(bool sts) = 0;
  virtual RPC_SRV_RESULT get_back_light(bool &sts) = 0;
};
#endif
