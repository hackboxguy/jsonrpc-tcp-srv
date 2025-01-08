// ADNetClient.hpp
#ifndef __ADNETCLIENT_H_
#define __ADNETCLIENT_H_

#include <ctime>
#include <string>

class Timer {
public:
  Timer() { clock_gettime(CLOCK_MONOTONIC, &beg_); }
  double elapsed() {
    clock_gettime(CLOCK_MONOTONIC, &end_);
    return end_.tv_sec - beg_.tv_sec +
           (end_.tv_nsec - beg_.tv_nsec) / 1000000000.;
  }
  void reset() { clock_gettime(CLOCK_MONOTONIC, &beg_); }

private:
  timespec beg_, end_;
};

class ADNetClient {
private:
  Timer CmdTimer;
  double time_elapsed;
  char ip[16]; // Fixed size for IPv4 string
  int port;
  bool connected;
  int sockfd;

public:
  ADNetClient();
  ADNetClient(std::string ip_addr, int port); // Keep std::string constructor
  ~ADNetClient();

  // Connection management with both std::string and const char* overloads
  int sock_connect(std::string ip_addr,
                   int port_num); // Original string version
  int sock_connect(const char *ip_addr,
                   int port_num); // New const char* version
  int sock_connect();
  int sock_disconnect();

  // Data transmission
  int send_data(char *buffer); // Original version for compatibility
  int send_data(const char *buffer, size_t length); // New safer version
  int receive_data(char *buffer, int buf_total_size);
  int receive_data_blocking(char *recv_buf, int buf_total_size, int timeout_ms);

  // Status and information
  int test_print();
  std::string get_ip_addr(); // Keep returning std::string for compatibility
  double get_communication_time_in_ms() const;
};

#endif
