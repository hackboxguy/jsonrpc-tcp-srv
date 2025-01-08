// ADNetClient.cpp
#include "ADNetClient.hpp"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

ADNetClient::ADNetClient() : time_elapsed(0), port(-1), connected(false) {
  ip[0] = '\0';
}

ADNetClient::ADNetClient(std::string ip_addr, int prt)
    : time_elapsed(0), port(prt), connected(false) {
  strncpy(ip, ip_addr.c_str(), sizeof(ip) - 1);
  ip[sizeof(ip) - 1] = '\0';
  sock_connect();
}

ADNetClient::~ADNetClient() { sock_disconnect(); }

int ADNetClient::test_print() {
  printf("This is ADNetClient ip=%s port=%d connected=%d\n", ip, port,
         connected);
  return 0;
}

std::string ADNetClient::get_ip_addr() { return std::string(ip); }

// Original string version
int ADNetClient::sock_connect(std::string ip_addr, int port_num) {
  return sock_connect(ip_addr.c_str(), port_num);
}

// New const char* version
int ADNetClient::sock_connect(const char *ip_addr, int port_num) {
  if (connected || port_num < 1 || !ip_addr || !ip_addr[0])
    return -1;

  strncpy(ip, ip_addr, sizeof(ip) - 1);
  ip[sizeof(ip) - 1] = '\0';
  port = port_num;
  return sock_connect();
}

int ADNetClient::sock_connect() {
  if (connected || port < 1 || !ip[0])
    return -1;

  struct sockaddr_in addr;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    return -1;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(ip);
  addr.sin_port = htons(port);

  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sockfd);
    return -1;
  }

  connected = true;
  return 0;
}

int ADNetClient::sock_disconnect() {
  if (connected) {
    close(sockfd);
    connected = false;
  }
  return 0;
}

int ADNetClient::send_data(char *send_buf) {
  if (!send_buf)
    return -1;
  return send_data(send_buf, strlen(send_buf) + 1); // +1 for null terminator
}

int ADNetClient::send_data(const char *send_buf, size_t length) {
  if (!connected || !send_buf || length == 0)
    return -1;

  CmdTimer.reset();
  size_t total_sent = 0;

  while (total_sent < length) {
    ssize_t sent = send(sockfd, send_buf + total_sent, length - total_sent, 0);
    if (sent <= 0)
      return -1;
    total_sent += sent;
  }

  return 0;
}

int ADNetClient::receive_data(char *recv_buf, int buf_total_size) {
  if (!connected || !recv_buf || buf_total_size <= 0)
    return -1;

  ssize_t received = recv(sockfd, recv_buf, buf_total_size - 1, 0);
  if (received >= 0) {
    recv_buf[received] = '\0';
  }
  return received;
}

int ADNetClient::receive_data_blocking(char *recv_buf, int buf_total_size,
                                       int timeout_ms) {
  if (!connected || !recv_buf || buf_total_size <= 0)
    return -1;

  int data = -1;
  int elapsed_ms = 0;
  const int sleep_us = 1000; // 1ms sleep

  CmdTimer.reset();
  while (elapsed_ms < timeout_ms) {
    data = recv(sockfd, recv_buf, buf_total_size - 1, 0);
    if (data >= 0) {
      recv_buf[data] = '\0';
      break;
    }
    usleep(sleep_us);
    elapsed_ms++;
  }

  time_elapsed = CmdTimer.elapsed();
  return data;
}

double ADNetClient::get_communication_time_in_ms() const {
  return time_elapsed;
}
