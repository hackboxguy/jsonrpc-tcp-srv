#ifndef __ADNETCLIENT_H_
#define __ADNETCLIENT_H_
#include <iostream>
using namespace std;
#include <ctime>
class Timer {
public:
  Timer() { clock_gettime(CLOCK_REALTIME, &beg_); }
  double elapsed() {
    clock_gettime(CLOCK_REALTIME, &end_);
    return end_.tv_sec - beg_.tv_sec +
           (end_.tv_nsec - beg_.tv_nsec) / 1000000000.;
  }
  void reset() { clock_gettime(CLOCK_REALTIME, &beg_); }

private:
  timespec beg_, end_;
};
class ADNetClient {
  Timer CmdTimer;
  double time_elapsed;
  string ip;
  int port;
  bool connected;
  int sockfd;
  bool initialized;

public:
  ADNetClient();
  ADNetClient(string, int);
  ~ADNetClient();
  int test_print();
  int sock_connect(string ip_addr, int port_num);
  int sock_connect();
  int sock_disconnect();
  int send_data(char *buffer);
  int receive_data(char *buffer, int buf_total_size);
  int receive_data_blocking(char *recv_buf, int buf_total_size, int timeout_ms);
  string get_ip_addr();
  double get_communication_time_in_ms();
};
#endif
