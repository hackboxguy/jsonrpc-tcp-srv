#include "DevIdentNexx3020.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
using namespace std;
/* ------------------------------------------------------------------------- */
DevIdentNexx3020::DevIdentNexx3020() {}
/* ------------------------------------------------------------------------- */
DevIdentNexx3020::~DevIdentNexx3020() {}
/* ------------------------------------------------------------------------- */
RPC_SRV_RESULT DevIdentNexx3020::device_identify() {
  char command[255];
  int res;
  // blink blue led on nexx3020f pocket router
  for (int i = 0; i < 10; i++) {
    sprintf(command,
            "echo 1 > /sys/class/leds/wt3020\\:blue\\:power/brightness");
    res = system(command);
    usleep(250000);
    sprintf(command,
            "echo 0 > /sys/class/leds/wt3020\\:blue\\:power/brightness");
    res = system(command);
    usleep(250000);
  }

  // switch the blue led to ON
  sprintf(command, "echo 0 > /sys/class/leds/wt3020\\:blue\\:power/brightness");
  if (system(command) != 0) {
    // pPanelReq->result=RPC_SRV_RESULT_FILE_OPEN_ERR;
    // return 0;
    ;
  }
  return RPC_SRV_RESULT_SUCCESS;
}
/* ------------------------------------------------------------------------- */
