// Stream framing and bounded client I/O against an in-process server
// (review findings C4, C7, C8).
#include "ADJsonRpcClient.hpp"
#include "adtest.hpp"
#include "test_net_util.hpp"
#include <pthread.h>
#include <set>

using namespace testnet;

namespace {
TestServer *server() {
  static TestServer *s = new TestServer(test_port());
  return s;
}
// reads n responses and returns the ids that were answered with a result
std::multiset<int> read_ids(int fd, int n, int timeout_ms = 5000) {
  std::multiset<int> ids;
  Reader r;
  std::string resp;
  for (int i = 0; i < n; i++) {
    if (!r.next_object(fd, resp, timeout_ms))
      break;
    int id;
    bool res;
    if (parse_response(resp, &id, &res) && res)
      ids.insert(id);
  }
  return ids;
}
std::multiset<int> range_ids(int from, int n) {
  std::multiset<int> s;
  for (int i = 0; i < n; i++)
    s.insert(from + i);
  return s;
}
} // namespace

// F1/F2: many requests back to back in one send()
TEST_CASE("C8: 1000 requests in one send get 1000 responses") {
  int fd = connect_to(server()->port);
  REQUIRE(fd >= 0);
  std::string all;
  for (int i = 0; i < 1000; i++)
    all += version_request(i);
  REQUIRE(send_all(fd, all));
  CHECK(read_ids(fd, 1000) == range_ids(0, 1000));
  close(fd);
}

// F3/F4: newline (NDJSON) and NUL separated requests
TEST_CASE("C8: newline and NUL separated requests") {
  int fd = connect_to(server()->port);
  REQUIRE(fd >= 0);
  std::string all;
  for (int i = 0; i < 50; i++) {
    all += version_request(i);
    all += (i % 2) ? std::string("\n") : std::string(1, '\0');
  }
  REQUIRE(send_all(fd, all));
  CHECK(read_ids(fd, 50) == range_ids(0, 50));
  close(fd);
}

// F5: "}{" inside a string value must not split the request
TEST_CASE("C8: }{ inside a string") {
  int fd = connect_to(server()->port);
  REQUIRE(fd >= 0);
  REQUIRE(send_all(fd,
                   "{\"jsonrpc\":\"2.0\",\"method\":\"get_rpc_srv_version\","
                   "\"params\":{\"msg\":\"a}{b\"},\"id\":77}"));
  CHECK(read_ids(fd, 1) == range_ids(77, 1));
  close(fd);
}

// F1: one request split over two sends with a gap
TEST_CASE("C8: request split across two sends") {
  int fd = connect_to(server()->port, true);
  REQUIRE(fd >= 0);
  std::string req = version_request(5);
  REQUIRE(send_all(fd, req.substr(0, 20)));
  usleep(50000);
  REQUIRE(send_all(fd, req.substr(20)));
  CHECK(read_ids(fd, 1) == range_ids(5, 1));
  close(fd);
}

// F1/F6: a 100 KB request delivered 1 byte per send()
TEST_CASE("C8: 100 KB request sent one byte at a time") {
  int fd = connect_to(server()->port, true);
  REQUIRE(fd >= 0);
  std::string req = "{\"jsonrpc\":\"2.0\",\"method\":\"get_rpc_srv_version\","
                    "\"params\":{\"pad\":\"" +
                    std::string(100 * 1024, 'p') + "\"},\"id\":21}";
  for (size_t i = 0; i < req.size(); i++)
    REQUIRE(send_all(fd, req.data() + i, 1));
  CHECK(read_ids(fd, 1, 10000) == range_ids(21, 1));
  close(fd);
}

TEST_CASE("C8: two clients interleave partial requests") {
  int a = connect_to(server()->port, true);
  int b = connect_to(server()->port, true);
  REQUIRE(a >= 0 && b >= 0);
  std::string ra = version_request(100), rb = version_request(200);
  for (size_t i = 0; i < ra.size() || i < rb.size(); i += 7) {
    if (i < ra.size())
      send_all(a, ra.substr(i, 7));
    if (i < rb.size())
      send_all(b, rb.substr(i, 7));
    usleep(1000);
  }
  CHECK(read_ids(a, 1) == range_ids(100, 1));
  CHECK(read_ids(b, 1) == range_ids(200, 1));
  close(a);
  close(b);
}

// stale partial data must not leak into the next connection on the same fd
TEST_CASE("C8: disconnect mid-request, next client starts clean") {
  for (int i = 0; i < 20; i++) {
    int fd = connect_to(server()->port);
    REQUIRE(fd >= 0);
    send_all(fd, "{\"jsonrpc\":\"2.0\",\"method\":\"get_rpc_");
    usleep(5000);
    close(fd);
    usleep(5000);
    REQUIRE(server_alive(server()->port));
  }
}

TEST_CASE("C8: garbage between requests closes only that connection") {
  int fd = connect_to(server()->port);
  REQUIRE(fd >= 0);
  send_all(fd, version_request(1) + "garbage" + version_request(2));
  Reader r;
  std::string resp;
  int got_result = 0, got_parse_error = 0;
  while (r.next_object(fd, resp, 2000)) {
    int id;
    bool res, err;
    if (parse_response(resp, &id, &res, &err)) {
      got_result += res;
      got_parse_error += err;
    }
  }
  CHECK_EQ(got_parse_error, 1);
  CHECK(server_alive(server()->port));
  close(fd);
}

namespace {
struct ClientArg {
  int port;
  int base;
  int count;
  int ok;
};
void *client_thread(void *p) {
  ClientArg *a = (ClientArg *)p;
  unsigned seed = a->base;
  int done = 0;
  while (done < a->count) {
    int fd = connect_to(a->port);
    if (fd < 0)
      continue;
    int batch = 1 + rand_r(&seed) % 20;
    if (done + batch > a->count)
      batch = a->count - done;
    std::string all;
    for (int i = 0; i < batch; i++)
      all += version_request(a->base + done + i);
    send_all(fd, all);
    std::multiset<int> ids = read_ids(fd, batch);
    if (ids == range_ids(a->base + done, batch))
      a->ok += batch;
    close(fd);
    done += batch;
  }
  return NULL;
}
} // namespace

// C7: responses must never reach another client, even when fds are reused
TEST_CASE("C7: 16 clients x 1000 requests, every response matches its id") {
  const int N = 16;
  pthread_t th[N];
  ClientArg args[N];
  for (int i = 0; i < N; i++) {
    args[i].port = server()->port;
    args[i].base = (i + 1) * 100000;
    args[i].count = 1000;
    args[i].ok = 0;
    pthread_create(&th[i], NULL, client_thread, &args[i]);
  }
  for (int i = 0; i < N; i++) {
    pthread_join(th[i], NULL);
    CHECK_EQ(args[i].ok, 1000);
  }
}

// C4: a peer that accepts and never replies must not block the client
TEST_CASE("C4: client call against a black-hole server times out") {
  int lfd = socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(lfd >= 0);
  int on = 1;
  setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(test_port(1));
  REQUIRE(bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
  REQUIRE(listen(lfd, 4) == 0);
  ADJsonRpcClient client;
  REQUIRE(client.rpc_server_connect("127.0.0.1", test_port(1)) == 0);
  long t0 = now_ms();
  char version[255] = "";
  RPC_SRV_RESULT res = client.get_string_type((char *)"get_rpc_srv_version",
                                              (char *)"version", version);
  long elapsed = now_ms() - t0;
  CHECK(res != RPC_SRV_RESULT_SUCCESS);
  CHECK(elapsed >= 3500 && elapsed < 6000); // 4000 ms receive timeout
  client.rpc_server_disconnect();
  close(lfd);
}

TEST_CASE("C4: connect to a closed port fails fast") {
  ADJsonRpcClient client;
  long t0 = now_ms();
  CHECK(client.rpc_server_connect("127.0.0.1", test_port(2)) != 0);
  CHECK(now_ms() - t0 < 1000);
}

TEST_CASE("C4: client receives a response larger than one segment") {
  ADJsonRpcClient client;
  REQUIRE(client.rpc_server_connect("127.0.0.1", server()->port) == 0);
  for (int i = 0; i < 200; i++) {
    char version[255] = "";
    CHECK(client.get_string_type((char *)"get_rpc_srv_version",
                                 (char *)"version",
                                 version) == RPC_SRV_RESULT_SUCCESS);
  }
  client.rpc_server_disconnect();
}

ADTEST_MAIN()
