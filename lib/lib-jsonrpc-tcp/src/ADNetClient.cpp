// ADNetClient.cpp
#include "ADNetClient.hpp"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static long monotonic_ms() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

ADNetClient::ADNetClient()
    : time_elapsed(0), port(-1), connected(false), sockfd(-1),
      connect_timeout_ms(AD_NET_CLIENT_CONNECT_TIMEOUT_MS) {
  ip[0] = '\0';
}

ADNetClient::ADNetClient(std::string ip_addr, int prt)
    : time_elapsed(0), port(prt), connected(false), sockfd(-1),
      connect_timeout_ms(AD_NET_CLIENT_CONNECT_TIMEOUT_MS) {
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
  sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (sockfd < 0)
    return -1;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(ip);
  addr.sin_port = htons(port);

  // non-blocking connect bounded by connect_timeout_ms
  int flags = fcntl(sockfd, F_GETFL, 0);
  if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
    close(sockfd);
    sockfd = -1;
    return -1;
  }
  int rc = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0 && errno == EINPROGRESS) {
    long deadline = monotonic_ms() + connect_timeout_ms;
    for (;;) {
      struct pollfd pfd;
      pfd.fd = sockfd;
      pfd.events = POLLOUT;
      pfd.revents = 0;
      long left = deadline - monotonic_ms();
      if (left < 0)
        left = 0;
      rc = poll(&pfd, 1, (int)left);
      if (rc < 0 && errno == EINTR)
        continue;
      if (rc <= 0) {
        rc = -1; // timeout or poll error
        break;
      }
      int so_error = 0;
      socklen_t optlen = sizeof(so_error);
      if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &optlen) < 0 ||
          so_error != 0)
        rc = -1;
      else
        rc = 0;
      break;
    }
  }
  if (rc < 0 || fcntl(sockfd, F_SETFL, flags) < 0) {
    close(sockfd);
    sockfd = -1;
    return -1;
  }
  struct timeval tv;
  tv.tv_sec = AD_NET_CLIENT_SEND_TIMEOUT_MS / 1000;
  tv.tv_usec = (AD_NET_CLIENT_SEND_TIMEOUT_MS % 1000) * 1000;
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  rx_framer.reset();
  connected = true;
  return 0;
}

int ADNetClient::sock_disconnect() {
  if (connected) {
    close(sockfd);
    sockfd = -1;
    connected = false;
  }
  rx_framer.reset();
  return 0;
}

int ADNetClient::send_data(char *send_buf) {
  if (!send_buf)
    return -1;
  // no trailing NUL: the stream is framed by the JSON structure. Servers
  // still accept NUL separators from older clients.
  return send_data(send_buf, strlen(send_buf));
}

int ADNetClient::send_data(const char *send_buf, size_t length) {
  if (!connected || !send_buf || length == 0)
    return -1;

  CmdTimer.reset();
  // a response that arrived after the previous call timed out must not be
  // taken as the answer to this request
  rx_framer.reset();
  char drain[1024];
  while (recv(sockfd, drain, sizeof(drain), MSG_DONTWAIT) > 0)
    ;

  size_t total_sent = 0;
  while (total_sent < length) {
    ssize_t sent =
        send(sockfd, send_buf + total_sent, length - total_sent, MSG_NOSIGNAL);
    if (sent < 0 && errno == EINTR)
      continue;
    if (sent <= 0)
      return -1; // includes SO_SNDTIMEO expiry
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

// 1: readable, 0: timeout, -1: error
int ADNetClient::wait_readable(int timeout_ms) {
  long deadline = monotonic_ms() + (timeout_ms > 0 ? timeout_ms : 0);
  for (;;) {
    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    long left = deadline - monotonic_ms();
    if (left < 0)
      left = 0;
    int rc = poll(&pfd, 1, (int)left);
    if (rc < 0 && errno == EINTR)
      continue;
    if (rc < 0)
      return -1;
    return rc > 0 ? 1 : 0;
  }
}

int ADNetClient::receive_data_blocking(char *recv_buf, int buf_total_size,
                                       int timeout_ms) {
  if (!connected || !recv_buf || buf_total_size <= 0)
    return -1;
  recv_buf[0] = '\0';

  int data = -1;
  CmdTimer.reset();
  if (wait_readable(timeout_ms) == 1) {
    do {
      data = recv(sockfd, recv_buf, buf_total_size - 1, MSG_DONTWAIT);
    } while (data < 0 && errno == EINTR);
    if (data > 0)
      recv_buf[data] = '\0';
    else {
      recv_buf[0] = '\0';
      data = -1; // peer closed or error
    }
  }

  time_elapsed = CmdTimer.elapsed();
  return data;
}

int ADNetClient::receive_json_blocking(std::string &out, int timeout_ms) {
  out.clear();
  if (!connected)
    return -1;
  CmdTimer.reset();
  long deadline = monotonic_ms() + (timeout_ms > 0 ? timeout_ms : 0);
  int result = -1;
  for (;;) {
    if (rx_framer.next(out)) {
      result = (int)out.size();
      break;
    }
    if (rx_framer.has_error()) {
      result = -2;
      break;
    }
    long left = deadline - monotonic_ms();
    if (left <= 0 || wait_readable((int)left) != 1)
      break; // timeout
    char chunk[4096];
    ssize_t rc = recv(sockfd, chunk, sizeof(chunk), MSG_DONTWAIT);
    if (rc < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
      continue;
    if (rc <= 0) {
      result = -2; // peer closed or socket error
      break;
    }
    rx_framer.feed(chunk, rc);
  }
  time_elapsed = CmdTimer.elapsed();
  return result;
}

int ADNetClient::receive_json_blocking(char *recv_buf, int buf_total_size,
                                       int timeout_ms) {
  if (!connected || !recv_buf || buf_total_size <= 0)
    return -1;
  recv_buf[0] = '\0';
  std::string msg;
  if (receive_json_blocking(msg, timeout_ms) < 0)
    return -1;
  size_t n = msg.size();
  if (n > (size_t)buf_total_size - 1)
    n = buf_total_size - 1;
  memcpy(recv_buf, msg.data(), n);
  recv_buf[n] = '\0';
  return (int)n;
}

double ADNetClient::get_communication_time_in_ms() const {
  return time_elapsed;
}
