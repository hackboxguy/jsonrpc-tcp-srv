// Negative request corpus against an in-process server (review section 5.4).
// Every malformed request must leave the server able to answer the next
// get_rpc_srv_version. Run under -DADLIB_SANITIZE=address to catch C1/C2/C3.
#include "adtest.hpp"
#include "test_net_util.hpp"

using namespace testnet;

namespace {
TestServer *server() {
  // intentionally never destroyed: shutdown ordering is tested separately
  static TestServer *s = new TestServer(test_port());
  return s;
}
// sends payload on its own connection, collects up to max_resp responses
std::vector<std::string> send_raw(const std::string &payload, int max_resp = 1,
                                  int timeout_ms = 1500) {
  std::vector<std::string> out;
  int fd = connect_to(server()->port);
  if (fd < 0)
    return out;
  send_all(fd, payload);
  Reader r;
  std::string resp;
  while ((int)out.size() < max_resp && r.next_object(fd, resp, timeout_ms))
    out.push_back(resp);
  close(fd);
  return out;
}
int rpc_error_code(const std::string &resp) {
  int code = 0;
  json_object *o = json_tokener_parse(resp.c_str());
  json_object *err = NULL, *c = NULL;
  if (o && json_object_object_get_ex(o, "error", &err) &&
      json_object_object_get_ex(err, "code", &c))
    code = json_object_get_int(c);
  if (o)
    json_object_put(o);
  return code;
}
} // namespace

TEST_CASE("server answers a valid request") {
  REQUIRE(server_alive(server()->port));
}

// C1: method name longer than JSON_RPC_METHOD_NAME_MAX_LENGTH
TEST_CASE("C1: 300 character method name") {
  std::string req = "{\"jsonrpc\":\"2.0\",\"method\":\"" +
                    std::string(300, 'A') + "\",\"id\":7}";
  std::vector<std::string> r = send_raw(req);
  REQUIRE(r.size() == 1);
  CHECK_EQ(rpc_error_code(r[0]), -32601);
  CHECK(server_alive(server()->port));
}

TEST_CASE("C1: 1 KB jsonrpc and access values") {
  std::string big(1024, 'x');
  std::string req = "{\"jsonrpc\":\"" + big +
                    "\",\"method\":\"get_rpc_srv_version\",\"access\":\"" +
                    big + "\",\"id\":8}";
  std::vector<std::string> r = send_raw(req);
  REQUIRE(r.size() == 1);
  CHECK(server_alive(server()->port));
}

// C1: find_single_param used to sprintf into a 255 byte stack buffer
TEST_CASE("C1: 10 KB string parameter") {
  std::string req = "{\"jsonrpc\":\"2.0\",\"method\":\"get_rpc_req_status\","
                    "\"params\":{\"taskId\":\"" +
                    std::string(10 * 1024, '9') + "\"},\"id\":9}";
  std::vector<std::string> r = send_raw(req);
  REQUIRE(r.size() == 1);
  CHECK(server_alive(server()->port));
}

// C2: every error reply used to release error_obj twice
TEST_CASE("C2: parse error reply") {
  std::vector<std::string> r = send_raw("{\"jsonrpc\":\"2.0\",\"method\":}");
  REQUIRE(r.size() == 1);
  CHECK_EQ(rpc_error_code(r[0]), -32700);
  CHECK(server_alive(server()->port));
}

TEST_CASE("C2: unknown method reply") {
  std::vector<std::string> r =
      send_raw("{\"jsonrpc\":\"2.0\",\"method\":\"no_such_method\",\"id\":3}");
  REQUIRE(r.size() == 1);
  CHECK_EQ(rpc_error_code(r[0]), -32601);
  int id;
  bool res;
  REQUIRE(parse_response(r[0], &id, &res));
  CHECK_EQ(id, 3);
  CHECK(server_alive(server()->port));
}

TEST_CASE("C2: missing id reply") {
  std::vector<std::string> r =
      send_raw("{\"jsonrpc\":\"2.0\",\"method\":\"get_rpc_srv_version\"}");
  REQUIRE(r.size() == 1);
  CHECK_EQ(rpc_error_code(r[0]), -32600);
  CHECK(server_alive(server()->port));
}

// C2: mapper failure (missing params) goes through the tmp_obj path
TEST_CASE("C2: missing parameter reply") {
  std::vector<std::string> r =
      send_raw("{\"jsonrpc\":\"2.0\",\"method\":\"get_rpc_req_status\","
               "\"id\":11}");
  REQUIRE(r.size() == 1);
  int id;
  bool res;
  REQUIRE(parse_response(r[0], &id, &res));
  CHECK_EQ(id, 11);
  CHECK(server_alive(server()->port));
}

TEST_CASE("repeated error replies do not corrupt the heap") {
  for (int i = 0; i < 200; i++) {
    std::vector<std::string> r = send_raw("{not json}");
    CHECK(r.size() == 1);
  }
  CHECK(server_alive(server()->port));
}

TEST_CASE("deeply nested JSON") {
  std::string req = "{\"jsonrpc\":\"2.0\",\"method\":\"get_rpc_srv_version\","
                    "\"params\":" +
                    std::string(500, '[') + std::string(500, ']') +
                    ",\"id\":12}";
  send_raw(req);
  CHECK(server_alive(server()->port));
}

TEST_CASE("JSON array batch is rejected") {
  std::vector<std::string> r =
      send_raw("[" + version_request(1) + "," + version_request(2) + "]");
  REQUIRE(r.size() >= 1);
  CHECK_EQ(rpc_error_code(r[0]), -32600);
  CHECK(server_alive(server()->port));
}

// C7: a full 64 KB recv() used to write one byte past receive_buffer
TEST_CASE("C7: request of exactly 65535 bytes") {
  std::string head = "{\"jsonrpc\":\"2.0\",\"method\":\"get_rpc_srv_version\","
                     "\"params\":{\"pad\":\"";
  std::string tail = "\"},\"id\":13}";
  std::string req =
      head + std::string(65535 - head.size() - tail.size(), 'p') + tail;
  REQUIRE(req.size() == 65535);
  send_raw(req, 4, 1000);
  CHECK(server_alive(server()->port));
}

TEST_CASE("70 KB request") {
  std::string req = "{\"jsonrpc\":\"2.0\",\"method\":\"get_rpc_srv_version\","
                    "\"params\":{\"pad\":\"" +
                    std::string(70 * 1024, 'p') + "\"},\"id\":14}";
  send_raw(req, 4, 1000);
  CHECK(server_alive(server()->port));
}

TEST_CASE("NUL bytes and invalid UTF-8") {
  std::string req("{\"jsonrpc\":\"2.0\",\"method\":\"get_rpc_srv_version\","
                  "\"params\":{\"s\":\"\xff\xfe\xc3\x28\"},\"id\":15}");
  req.push_back('\0');
  req.append(std::string("\0\0\0", 3));
  send_raw(req, 2, 1000);
  CHECK(server_alive(server()->port));
}

TEST_CASE("client disconnects before reading the response") {
  for (int i = 0; i < 50; i++) {
    int fd = connect_to(server()->port);
    REQUIRE(fd >= 0);
    send_all(fd, version_request(i));
    close(fd);
  }
  CHECK(server_alive(server()->port));
}

ADTEST_MAIN()
