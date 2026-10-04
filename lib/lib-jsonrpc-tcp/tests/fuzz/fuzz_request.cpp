// libFuzzer target: the complete server request path (framer, Proxy
// parsing, mapper, built-in RPC handlers, response path) of an in-process
// ADJsonRpcMgr. Each input is sent on its own connection; a request on a
// second connection then waits until the RPC thread has caught up, so
// inputs do not pile up. Crashes in any server thread abort the process.
#include "test_net_util.hpp"
#include <stdint.h>

using namespace testnet;

static TestServer *srv = NULL;
static int control_fd = -1;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
  srv = new TestServer(test_port());
  return 0;
}

// Waits until the server answered a request sent after the input. Inputs
// can legitimately keep the RPC thread busy for a while (reset_task_status
// waits up to 1 s per call while tasks run), so slow answers are only
// reported; no answer within 30 s means the server is stuck.
static void sync_with_server(const uint8_t *data, size_t size) {
  static int marker = 0;
  long t0 = now_ms();
  while (now_ms() - t0 < 30000) {
    if (control_fd < 0)
      control_fd = connect_to(srv->port);
    if (control_fd < 0) {
      usleep(10000);
      continue;
    }
    int id = 1000000 + (marker++ % 1000000);
    if (send_all(control_fd, version_request(id))) {
      Reader r;
      std::string resp;
      long left = 30000 - (now_ms() - t0);
      if (left > 0 && r.next_object(control_fd, resp, (int)left)) {
        long took = now_ms() - t0;
        if (took > 2000)
          fprintf(stderr, "slow sync: %ld ms after a %zu byte input\n", took,
                  size);
        return;
      }
    }
    close(control_fd);
    control_fd = -1;
  }
  fprintf(stderr, "server did not answer for 30 s\n");
  abort();
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  int fd = connect_to(srv->port);
  if (fd < 0)
    abort();
  send_all(fd, (const char *)data, size);
  shutdown(fd, SHUT_WR);
  // read whatever comes back until the server closes or goes quiet
  char buf[4096];
  struct pollfd p;
  p.fd = fd;
  p.events = POLLIN;
  while (poll(&p, 1, 50) > 0 && recv(fd, buf, sizeof(buf), 0) > 0)
    ;
  close(fd);
  sync_with_server(data, size);
  return 0;
}
