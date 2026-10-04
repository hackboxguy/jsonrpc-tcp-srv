// ADNetClient.hpp
#ifndef __ADNETCLIENT_H_
#define __ADNETCLIENT_H_

#include "ADJsonStreamFramer.hpp"
#include <ctime>
#include <string>
// default bound for connect(); a blocking connect() can otherwise take
// about two minutes (SYN retries) against a dead peer
#define AD_NET_CLIENT_CONNECT_TIMEOUT_MS 3000
// bound for a single blocking send()
#define AD_NET_CLIENT_SEND_TIMEOUT_MS 5000

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
  int connect_timeout_ms;
  ADJsonStreamFramer rx_framer; // assembles responses split across segments
  int wait_readable(int timeout_ms);

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
  // waits up to timeout_ms for data and returns what one recv() delivered;
  // -1 on timeout, error or peer close (recv_buf is then an empty string)
  int receive_data_blocking(char *recv_buf, int buf_total_size, int timeout_ms);
  // waits up to timeout_ms for one complete JSON object/array (it may arrive
  // in several segments) and copies it NUL terminated into recv_buf
  // (truncated if larger). Returns its length, or -1 on timeout, error, peer
  // close or a framing error.
  int receive_json_blocking(char *recv_buf, int buf_total_size, int timeout_ms);
  // same without truncation; returns the length, -1 on timeout or error,
  // -2 if the peer closed the connection or sent something that is not JSON
  int receive_json_blocking(std::string &out, int timeout_ms);
  bool is_connected() const { return connected; }
  void set_connect_timeout(int timeout_ms) { connect_timeout_ms = timeout_ms; }

  // Status and information
  int test_print();
  std::string get_ip_addr(); // Keep returning std::string for compatibility
  double get_communication_time_in_ms() const;
};

#endif
