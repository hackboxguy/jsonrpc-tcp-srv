// test_net_util.hpp - raw TCP helpers and an in-process RPC server for tests.
#ifndef __TEST_NET_UTIL_HPP_
#define __TEST_NET_UTIL_HPP_
#include "ADJsonRpcMgr.hpp"
#include <arpa/inet.h>
#include <errno.h>
#include <json-c/json.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

namespace testnet {
inline long now_ms() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}
inline bool port_is_free(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return false;
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  bool ok = bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0;
  close(fd);
  return ok;
}
// A block of 4 ports per test process, below the kernel's ephemeral range
// (32768+) so client connections of other tests cannot occupy them. The
// first free block starting at a pid based offset is used.
inline int pick_port_base() {
  int start = (int)(getpid() % 2000);
  for (int i = 0; i < 2000; i++) {
    int candidate = 22000 + ((start + i) % 2000) * 4;
    if (port_is_free(candidate) && port_is_free(candidate + 1) &&
        port_is_free(candidate + 2) && port_is_free(candidate + 3))
      return candidate;
  }
  return 22000;
}
inline int test_port(int offset = 0) {
  static const int base = pick_port_base(); // thread-safe init (C++11)
  return base + offset;
}
inline int connect_to(int port, bool nodelay = false) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(port);
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close(fd);
    return -1;
  }
  if (nodelay) {
    int on = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  }
  return fd;
}
inline bool send_all(int fd, const char *buf, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    ssize_t rc = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
    if (rc <= 0) {
      if (rc < 0 && errno == EINTR)
        continue;
      return false;
    }
    sent += rc;
  }
  return true;
}
inline bool send_all(int fd, const std::string &s) {
  return send_all(fd, s.data(), s.size());
}
// Reads complete top-level JSON objects from fd (brace counting outside
// strings) until 'count' objects arrived or timeout_ms expired.
class Reader {
  std::string buf;
  size_t scan;
  int depth;
  bool in_str, esc;
  size_t start;

public:
  Reader() : scan(0), depth(0), in_str(false), esc(false), start(0) {}
  bool next_object(int fd, std::string &out, int timeout_ms) {
    long deadline = now_ms() + timeout_ms;
    for (;;) {
      for (; scan < buf.size(); scan++) {
        char c = buf[scan];
        if (in_str) {
          if (esc)
            esc = false;
          else if (c == '\\')
            esc = true;
          else if (c == '"')
            in_str = false;
          continue;
        }
        if (c == '"')
          in_str = true;
        else if (c == '{' || c == '[') {
          if (depth++ == 0)
            start = scan;
        } else if ((c == '}' || c == ']') && depth > 0) {
          if (--depth == 0) {
            out = buf.substr(start, scan - start + 1);
            buf.erase(0, scan + 1);
            scan = 0;
            return true;
          }
        }
      }
      long left = deadline - now_ms();
      if (left <= 0)
        return false;
      struct pollfd p;
      p.fd = fd;
      p.events = POLLIN;
      int rc = poll(&p, 1, (int)left);
      if (rc <= 0)
        continue;
      char tmp[4096];
      ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
      if (n <= 0)
        return false;
      buf.append(tmp, n);
    }
  }
};
// parses a response and returns its id (or -1) and whether it has "result"
inline bool parse_response(const std::string &resp, int *id, bool *has_result,
                           bool *has_error = NULL) {
  json_object *o = json_tokener_parse(resp.c_str());
  if (o == NULL)
    return false;
  json_object *jid = NULL;
  *id = -1;
  if (json_object_object_get_ex(o, "id", &jid) && jid != NULL)
    *id = json_object_get_int(jid);
  *has_result = json_object_object_get_ex(o, "result", NULL);
  if (has_error)
    *has_error = json_object_object_get_ex(o, "error", NULL);
  json_object_put(o);
  return true;
}
inline std::string version_request(int id) {
  char buf[128];
  snprintf(buf, sizeof(buf),
           "{\"jsonrpc\":\"2.0\",\"method\":\"get_rpc_srv_version\",\"id\":%d}",
           id);
  return buf;
}
// sends one get_rpc_srv_version on a fresh connection; true if answered
inline bool server_alive(int port, int timeout_ms = 3000) {
  int fd = connect_to(port);
  if (fd < 0)
    return false;
  bool ok = false;
  if (send_all(fd, version_request(4242))) {
    Reader r;
    std::string resp;
    int id;
    bool res;
    if (r.next_object(fd, resp, timeout_ms) &&
        parse_response(resp, &id, &res) && id == 4242 && res)
      ok = true;
  }
  close(fd);
  return ok;
}
// minimal service: only the built-in ADJsonRpcMgr methods
struct TestServer {
  ADJsonRpcMgr mgr;
  int port;
  explicit TestServer(int p, int socket_log = 0)
      : mgr(1, false, NULL), port(p) {
    mgr.Start(port, socket_log, 0);
    // wait until the listen thread accepts connections
    for (int i = 0; i < 200; i++) {
      int fd = connect_to(port);
      if (fd >= 0) {
        close(fd);
        break;
      }
      usleep(10000);
    }
  }
};
// Accepts connections on 'port' and answers each framed request with the
// replies produced by 'script' (called with the request and its id).
struct FakeServer {
  int lfd;
  int port;
  pthread_t th;
  int stop;
  // returns the raw bytes to send back; "" = send nothing; "CLOSE" = close
  std::string (*script)(const std::string &req, int id, int n);
  std::string last_request;
  int requests;
  pthread_mutex_t m;
  FakeServer(int p, std::string (*s)(const std::string &, int, int))
      : port(p), stop(0), script(s), requests(0) {
    pthread_mutex_init(&m, NULL);
    lfd = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);
    bind(lfd, (struct sockaddr *)&addr, sizeof(addr));
    listen(lfd, 4);
    pthread_create(&th, NULL, run, this);
  }
  ~FakeServer() {
    __atomic_store_n(&stop, 1, __ATOMIC_SEQ_CST);
    shutdown(lfd, SHUT_RDWR);
    close(lfd);
    pthread_join(th, NULL);
  }
  static void *run(void *p) {
    FakeServer *f = (FakeServer *)p;
    while (!__atomic_load_n(&f->stop, __ATOMIC_SEQ_CST)) {
      int fd = accept(f->lfd, NULL, NULL);
      if (fd < 0)
        break;
      Reader r;
      std::string req;
      bool open = true;
      while (open && r.next_object(fd, req, 2000)) {
        json_object *o = json_tokener_parse(req.c_str());
        json_object *jid = NULL;
        int id = -1;
        if (o && json_object_object_get_ex(o, "id", &jid))
          id = json_object_get_int(jid);
        if (o)
          json_object_put(o);
        pthread_mutex_lock(&f->m);
        f->last_request = req;
        int n = ++f->requests;
        pthread_mutex_unlock(&f->m);
        std::string reply = f->script(req, id, n);
        if (reply == "CLOSE")
          open = false;
        else if (!reply.empty())
          send_all(fd, reply);
      }
      close(fd);
    }
    return NULL;
  }
};
// a JSON-RPC result reply with "return":"Success" and one string member
inline std::string result_reply(int id, const std::string &value) {
  return "{\"jsonrpc\":\"2.0\",\"result\":{\"return\":\"Success\","
         "\"message\":\"" +
         value + "\"},\"id\":" + std::to_string(id) + "}";
}
} // namespace testnet
#endif
