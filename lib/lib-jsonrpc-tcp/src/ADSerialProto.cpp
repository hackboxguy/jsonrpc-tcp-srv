#include "ADSerialProto.hpp"
#include <iostream>
using namespace std;
int ADSerialProto::timer_notification() { return 0; }
int ADSerialProto::sigio_notification() { return 0; }
int ADSerialProto::custom_sig_notification(int signum) { return 0; }
int ADSerialProto::notify_data_arrival(ADGenericChain *pRxChain,
                                       ADSerialProducer *pObj) {
  return 0;
}
ADSerialProto::ADSerialProto() {
  SerialHandler.subscribe_data_arrival(this);
  pMyTimer = NULL;
}
ADSerialProto::~ADSerialProto() { SerialHandler.stop(); }
int ADSerialProto::attach_timer_and_signal_notifier(ADTimer *pTimer) {
  pTimer->subscribe_timer_notification(this);
  SerialHandler.attach_timer_and_signal_notifier(pTimer);
  pMyTimer = pTimer;
  return 0;
}
int ADSerialProto::connect_uart(int baud, char *dev_node) {
  if (pMyTimer == NULL)
    return -1;
  SerialHandler.set_serial_port_settings(baud, dev_node);
  return SerialHandler.start();
}
