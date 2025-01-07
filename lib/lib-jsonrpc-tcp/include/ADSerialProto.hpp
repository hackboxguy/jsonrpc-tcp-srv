#ifndef __AD_SERIAL_PROTO_H_
#define __AD_SERIAL_PROTO_H_
#include "ADSerial.hpp"
#include <stdint.h>
#define SDRPC_MAX_UART_DATA_LENGTH 240
#define SDRPC_UART_FRAME_START 0xFE
#define SDRPC_UART_FRAME_STOP 0xFF
#define SDRPC_UART_FRAME_ESCAPE 0x80
typedef enum SDRPC_SERIAL_PKT_TYPE_T {
  SDRPC_SERIAL_PKT_REQ = 0,
  SDRPC_SERIAL_PKT_RES,
  SDRPC_SERIAL_PKT_NOTIFY,
  SDRPC_SERIAL_PKT_UNKNOWN,
  SDRPC_SERIAL_PKT_NONE
} SDRPC_SERIAL_PKT_TYPE;
typedef enum SDRPC_SERIAL_PKT_CMD_T {
  SDRPC_SERIAL_PKT_CMD_USRCMD = 0,
  SDRPC_SERIAL_PKT_CMD_RESEND,
  SDRPC_SERIAL_PKT_CMD_UNKNOWN,
  SDRPC_SERIAL_PKT_CMD_NONE
} SDRPC_SERIAL_PKT_CMD;
typedef struct SDRPC_SERIAL_PKT_T {
  uint16_t HeaderCS;
  SDRPC_SERIAL_PKT_TYPE PktType;
  uint32_t ID;
  SDRPC_SERIAL_PKT_CMD Cmd;
  uint8_t DataSize;
  uint16_t DataCS;
  uint8_t Data[SDRPC_MAX_UART_DATA_LENGTH];
} SDRPC_SERIAL_PKT;
class ADSerialProto : public ADTimerConsumer, public ADCSerialConsumer {
  ADTimer *pMyTimer;
  ADSerial SerialHandler;
  virtual int timer_notification();
  virtual int sigio_notification();
  virtual int custom_sig_notification(int signum);
  virtual int notify_data_arrival(ADGenericChain *pRxChain,
                                  ADSerialProducer *pObj);

public:
  ADSerialProto();
  ~ADSerialProto();
  int attach_timer_and_signal_notifier(ADTimer *pTimer);
  int connect_uart(int baud, char *dev_node);
};
#endif
