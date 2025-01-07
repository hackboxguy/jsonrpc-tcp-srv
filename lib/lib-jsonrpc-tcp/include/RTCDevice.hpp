#ifndef __RTCDEVICE_H_
#define __RTCDEVICE_H_
#include "ADCommon.hpp"
#include "I2CBusAccess.h"
#include <iostream>
#include <stdint.h>
#include <time.h>
class RTCDevice : public I2CBusAccess {
public:
  RTCDevice(std::string devnode);
  ~RTCDevice();
  virtual RPC_SRV_RESULT get_date_time(struct tm &time) = 0;
  virtual RPC_SRV_RESULT set_date_time(struct tm time) = 0;
};
#endif
