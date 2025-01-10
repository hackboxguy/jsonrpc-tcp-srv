#include "DevIdentA5V11.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
using namespace std;
/* ------------------------------------------------------------------------- */
DevIdentA5V11::DevIdentA5V11() {
  // on startup keep red led off to save power
  char command[255];
  int res;
  sprintf(command, "echo 0 > /sys/class/leds/a5-v11\\:red\\:power/brightness");
  res = system(command);
  if (res != 0)
    printf("DevIdentA5V11::DevIdentA5V11: system() failed\n");
}
/* ------------------------------------------------------------------------- */
DevIdentA5V11::~DevIdentA5V11() {}
/* ------------------------------------------------------------------------- */
RPC_SRV_RESULT DevIdentA5V11::device_identify() {
  char command[255];
  int res;
  // blink red led on a5-v11 pocket router
  for (int i = 0; i < 10; i++) {
    sprintf(command,
            "echo 1 > /sys/class/leds/a5-v11\\:red\\:power/brightness");
    res = system(command);
    usleep(250000);
    sprintf(command,
            "echo 0 > /sys/class/leds/a5-v11\\:red\\:power/brightness");
    res = system(command);
    usleep(250000);
  }

  // keep the red led in OFF condition
  sprintf(command, "echo 0 > /sys/class/leds/a5-v11\\:red\\:power/brightness");
  if (system(command) != 0) {
    // pPanelReq->result=RPC_SRV_RESULT_FILE_OPEN_ERR;
    // return 0;
    ;
  }
  return RPC_SRV_RESULT_SUCCESS;
}
/* ------------------------------------------------------------------------- */
