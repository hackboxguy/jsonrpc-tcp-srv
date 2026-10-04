// ADJsonRpcClient against a scripted fake server (findings V2-C2, V2-M2).
// Run under ASan: requests and responses larger than the old 1400-byte
// buffers and result strings larger than the caller's 255-byte buffer.
#include "ADJsonRpcClient.hpp"
#include "adtest.hpp"
#include "test_net_util.hpp"
#include <pthread.h>

using namespace testnet;

namespace {
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
std::string result_reply(int id, const std::string &value) {
  return "{\"jsonrpc\":\"2.0\",\"result\":{\"return\":\"Success\","
         "\"message\":\"" +
         value + "\"},\"id\":" + std::to_string(id) + "}";
}
std::string big_value(const std::string &, int id, int) {
  return result_reply(id, std::string(1100, 'v'));
}
std::string huge_value(const std::string &, int id, int) {
  return result_reply(id, std::string(10000, 'h'));
}
std::string echo_len(const std::string &req, int id, int) {
  return result_reply(id, std::to_string(req.size()));
}
// first answer has a stale id (a late reply to an earlier request)
std::string stale_first(const std::string &, int id, int) {
  return result_reply(id - 1, "stale") + "\n" + result_reply(id, "fresh");
}
// closes the connection after answering the first request
std::string close_after_first(const std::string &, int id, int n) {
  if (n == 2)
    return "CLOSE";
  return result_reply(id, "ok" + std::to_string(n));
}
} // namespace

TEST_CASE("V2-C2: 1100-byte result string into a 255-byte caller buffer") {
  FakeServer srv(test_port(), big_value);
  ADJsonRpcClient c;
  REQUIRE(c.rpc_server_connect("127.0.0.1", test_port()) == 0);
  char out[255];
  memset(out, 'X', sizeof(out));
  CHECK(c.get_string_type((char *)"m", (char *)"message", out) ==
        RPC_SRV_RESULT_SUCCESS);
  CHECK_EQ(strlen(out), (size_t)254); // truncated, NUL terminated
  c.rpc_server_disconnect();
}

TEST_CASE("V2-C2: set_output_size allows larger caller buffers") {
  FakeServer srv(test_port(), big_value);
  ADJsonRpcClient c;
  c.set_output_size(JSON_RPC_METHOD_RESP_MAX_LENGTH);
  REQUIRE(c.rpc_server_connect("127.0.0.1", test_port()) == 0);
  char out[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  CHECK(c.get_string_type((char *)"m", (char *)"message", out) ==
        RPC_SRV_RESULT_SUCCESS);
  CHECK_EQ(strlen(out), (size_t)JSON_RPC_METHOD_RESP_MAX_LENGTH - 1);
  c.rpc_server_disconnect();
}

// used to be cut at 1400 bytes: the parse failed and the call reported
// UNKNOWN although the server succeeded
TEST_CASE("V2-C2: 10 KB response is parsed completely") {
  FakeServer srv(test_port(), huge_value);
  ADJsonRpcClient c;
  REQUIRE(c.rpc_server_connect("127.0.0.1", test_port()) == 0);
  char out[255];
  CHECK(c.get_string_type((char *)"m", (char *)"message", out) ==
        RPC_SRV_RESULT_SUCCESS);
  CHECK(c.last_response().size() > 10000);
  c.rpc_server_disconnect();
}

// the old code strcpy'd the request into a 1400-byte send_buffer
TEST_CASE("V2-C2: request with a 5 KB string argument is sent whole") {
  FakeServer srv(test_port(), echo_len);
  ADJsonRpcClient c;
  REQUIRE(c.rpc_server_connect("127.0.0.1", test_port()) == 0);
  std::string msg(5000, 'm');
  char out[255];
  CHECK(c.set_double_string_get_single_string_type(
            (char *)"smssend", (char *)"number", (char *)"123",
            (char *)"message", (char *)msg.c_str(), (char *)"message",
            out) == RPC_SRV_RESULT_SUCCESS);
  CHECK(atoi(out) > 5000); // the server saw the whole request
  c.rpc_server_disconnect();
}

TEST_CASE("V2-M2: a response with another id is skipped") {
  FakeServer srv(test_port(), stale_first);
  ADJsonRpcClient c;
  REQUIRE(c.rpc_server_connect("127.0.0.1", test_port()) == 0);
  char out[255];
  CHECK(c.get_string_type((char *)"m", (char *)"message", out) ==
        RPC_SRV_RESULT_SUCCESS);
  CHECK(strcmp(out, "fresh") == 0);
  c.rpc_server_disconnect();
}

TEST_CASE("V2-M2: reconnect after the peer closed the connection") {
  FakeServer srv(test_port(), close_after_first);
  ADJsonRpcClient c;
  c.set_receive_timeout(1000);
  REQUIRE(c.rpc_server_connect("127.0.0.1", test_port()) == 0);
  char out[255];
  CHECK(c.get_string_type((char *)"m", (char *)"message", out) ==
        RPC_SRV_RESULT_SUCCESS);
  // the server closes instead of answering the second request
  CHECK(c.get_string_type((char *)"m", (char *)"message", out) !=
        RPC_SRV_RESULT_SUCCESS);
  // the third call reconnects and is answered
  CHECK(c.get_string_type((char *)"m", (char *)"message", out) ==
        RPC_SRV_RESULT_SUCCESS);
  CHECK(strcmp(out, "ok3") == 0);
  c.rpc_server_disconnect();
}

TEST_CASE("receive timeout setter bounds a call to a silent server") {
  FakeServer srv(test_port(),
                 [](const std::string &, int, int) { return std::string(); });
  ADJsonRpcClient c;
  c.set_receive_timeout(300);
  REQUIRE(c.rpc_server_connect("127.0.0.1", test_port()) == 0);
  char out[255];
  long t0 = now_ms();
  CHECK(c.get_string_type((char *)"m", (char *)"message", out) !=
        RPC_SRV_RESULT_SUCCESS);
  CHECK(now_ms() - t0 < 1000);
  c.rpc_server_disconnect();
}

ADTEST_MAIN()
