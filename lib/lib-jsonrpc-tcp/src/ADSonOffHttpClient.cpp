#include "ADSonOffHttpClient.hpp"
#include "ADCmnCltCmdProcessor.hpp"
#include "JsonCmnDef.h"
#include <iostream>
#include <sstream>
#include <string.h>
using namespace std;
ADSonOffHttpClient::ADSonOffHttpClient() {
  connected = false;
  memset(recv_buffer, 0, sizeof(recv_buffer) * sizeof(char));
  memset(send_buffer, 0, sizeof(send_buffer) * sizeof(char));
}
ADSonOffHttpClient::~ADSonOffHttpClient() { ClientSocket.sock_disconnect(); }
RPC_SRV_RESULT ADSonOffHttpClient::rpc_server_connect(string ip_addr,
                                                      int port_num) {
  if (connected == true)
    return RPC_SRV_RESULT_SUCCESS;
  if (ClientSocket.sock_connect(ip_addr, port_num) == 0) {
    connected = true;
    return RPC_SRV_RESULT_SUCCESS;
  } else
    return RPC_SRV_RESULT_FAIL;
}
RPC_SRV_RESULT ADSonOffHttpClient::rpc_server_disconnect(void) {
  if (connected)
    ClientSocket.sock_disconnect();
  connected = false;
  return RPC_SRV_RESULT_SUCCESS;
}
string ADSonOffHttpClient::get_ip_addr() { return ClientSocket.get_ip_addr(); }
RPC_SRV_RESULT ADSonOffHttpClient::set_sonoff_toggle() {
  if (!connected)
    return RPC_SRV_RESULT_HOST_NOT_REACHABLE_ERR;
  sprintf((char *)send_buffer,
          "GET /cm?cmnd=Power%%20TOGGLE HTTP/1.1\r\n Host:%s\r\n\r\n",
          ClientSocket.get_ip_addr().c_str());
  ClientSocket.send_data(send_buffer);
  ClientSocket.receive_data_blocking(recv_buffer, sizeof(recv_buffer), 4000);
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADSonOffHttpClient::get_sonoff_state(SONOFF_STATE &state) {
  state = SONOFF_STATE_UNKNOWN;
  if (!connected)
    return RPC_SRV_RESULT_HOST_NOT_REACHABLE_ERR;
  sprintf((char *)send_buffer,
          "GET /cm?cmnd=Power%%20 HTTP/1.1\r\n Host:%s\r\n\r\n",
          ClientSocket.get_ip_addr().c_str());
  ClientSocket.send_data(send_buffer);
  int received = ClientSocket.receive_data_blocking(recv_buffer,
                                                    sizeof(recv_buffer), 4000);
  recv_buffer[100] = '\0';
  received = ClientSocket.receive_data_blocking(recv_buffer,
                                                sizeof(recv_buffer), 4000);
  recv_buffer[100] = '\0';
  std::string msg = recv_buffer;
  std::string arg1, arg2, arg3, arg4, arg5, arg6;
  stringstream msgstream(msg);
  msgstream >> arg1;
  msgstream >> arg2;
  msgstream >> arg3;
  msgstream >> arg4;
  msgstream >> arg5;
  msgstream >> arg6;
  if (arg6 == "ON")
    state = SONOFF_STATE_ON;
  else if (arg6 == "OFF")
    state = SONOFF_STATE_OFF;
  else
    state = SONOFF_STATE_UNKNOWN;
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADSonOffHttpClient::set_sonoff_state(SONOFF_STATE state) {
  if (!connected)
    return RPC_SRV_RESULT_HOST_NOT_REACHABLE_ERR;
  if (state == SONOFF_STATE_OFF)
    sprintf((char *)send_buffer,
            "GET /cm?cmnd=Power%%20off HTTP/1.1\r\n Host:%s\r\n\r\n",
            ClientSocket.get_ip_addr().c_str());
  else if (state == SONOFF_STATE_ON)
    sprintf((char *)send_buffer,
            "GET /cm?cmnd=Power%%20On HTTP/1.1\r\n Host:%s\r\n\r\n",
            ClientSocket.get_ip_addr().c_str());
  else {
    return RPC_SRV_RESULT_ARG_ERROR;
  }
  ClientSocket.send_data(send_buffer);
  ClientSocket.receive_data_blocking(recv_buffer, sizeof(recv_buffer), 4000);
  return RPC_SRV_RESULT_SUCCESS;
}
