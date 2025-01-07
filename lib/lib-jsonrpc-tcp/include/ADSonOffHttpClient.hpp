#ifndef __ADSONOFFHTTPCLIENT_H_
#define __ADSONOFFHTTPCLIENT_H_
#include "ADCmnStringProcessor.hpp"
#include "ADJsonRpcProxy.hpp"
#include "ADNetClient.hpp"
#define MAX_RECV_BUFFER_SIZE 1400
#define MAX_SEND_BUFFER_SIZE 1400
typedef enum SONOFF_STATE_T {
  SONOFF_STATE_OFF = 0,
  SONOFF_STATE_ON,
  SONOFF_STATE_UNKNOWN,
  SONOFF_STATE_NONE
} SONOFF_STATE;
class ADSonOffHttpClient : public ADCmnStringProcessor {
public:
  bool connected;
  ADNetClient ClientSocket;
  char send_buffer[MAX_SEND_BUFFER_SIZE];
  char recv_buffer[MAX_RECV_BUFFER_SIZE];
  ADSonOffHttpClient();
  ~ADSonOffHttpClient();
  string get_ip_addr();
  RPC_SRV_RESULT rpc_server_connect(string ip_addr, int port_num);
  RPC_SRV_RESULT rpc_server_disconnect(void);
  RPC_SRV_RESULT set_sonoff_toggle();
  RPC_SRV_RESULT get_sonoff_state(SONOFF_STATE &state);
  RPC_SRV_RESULT set_sonoff_state(SONOFF_STATE state);
};
#endif
