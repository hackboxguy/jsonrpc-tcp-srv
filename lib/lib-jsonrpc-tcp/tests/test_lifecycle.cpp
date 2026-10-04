// Lifecycle tests: shutdown under load (H2) and the task worker (H3, H4).
#include "ADTaskWorker.hpp"
#include "adtest.hpp"
#include "test_net_util.hpp"
#include <pthread.h>
#include <vector>

using namespace testnet;

namespace {
struct LoadArg {
  int port;
  int stop;
  int answered;
  const char *method; // second method of the mix
  LoadArg(int p, const char *m = "trigger_settings_save")
      : port(p), stop(0), answered(0), method(m) {}
};
void *load_client(void *p) {
  LoadArg *a = (LoadArg *)p;
  int id = 0;
  while (!__atomic_load_n(&a->stop, __ATOMIC_SEQ_CST)) {
    int fd = connect_to(a->port);
    if (fd < 0) {
      usleep(1000);
      continue;
    }
    // mix in async tasks so the task worker calls back into the manager
    // while it is being destroyed
    std::string all;
    for (int i = 0; i < 10; i++) {
      if (i % 2)
        all += version_request(id++);
      else {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"id\":%d}", a->method,
                 id++);
        all += buf;
      }
    }
    send_all(fd, all);
    Reader r;
    std::string resp;
    while (r.next_object(fd, resp, 200))
      __atomic_add_fetch(&a->answered, 1, __ATOMIC_SEQ_CST);
    close(fd);
  }
  return NULL;
}
} // namespace

// H2: destroying the manager while requests are in flight used to run
// threads into already destroyed members
TEST_CASE("H2: destroy the RPC manager under load, 20 times") {
  int port = test_port();
  for (int round = 0; round < 20; round++) {
    ADJsonRpcMgr *mgr = new ADJsonRpcMgr(1, false, NULL);
    REQUIRE(mgr->Start(port, 0, 0) == 0);
    const int N = 8;
    pthread_t th[N];
    LoadArg arg(port);
    for (int i = 0; i < N; i++)
      pthread_create(&th[i], NULL, load_client, &arg);
    usleep(50000);
    long t0 = now_ms();
    delete mgr;
    long stop_ms = now_ms() - t0;
    CHECK(stop_ms < 2000);
    __atomic_store_n(&arg.stop, 1, __ATOMIC_SEQ_CST);
    for (int i = 0; i < N; i++)
      pthread_join(th[i], NULL);
    CHECK(arg.answered > 0);
  }
}

namespace {
// a service RPC handler; every call takes a little time on ReqThread
class SlowHandler : public ADJsonRpcMgrConsumer {
public:
  int calls;
  SlowHandler()
      : ADJsonRpcMgrConsumer("test_slow", 0, false, false), calls(0) {}
  virtual int MapJsonToBinary(JsonDataCommObj *pReq, int idx) {
    __atomic_add_fetch(&calls, 1, __ATOMIC_SEQ_CST);
    usleep(200);
    return -1; // answered with an error reply, enough for the test
  }
  virtual int MapBinaryToJson(JsonDataCommObj *pReq, int idx) { return 0; }
  virtual int ProcessWork(JsonDataCommObj *pReq, int idx,
                          ADJsonRpcMgrProducer *pObj) {
    return 0;
  }
  virtual RPC_SRV_RESULT ProcessWorkAsync(int idx, unsigned char *pData) {
    return RPC_SRV_RESULT_SUCCESS;
  }
  virtual void ReceiveEvent(int cltToken, int evntNum, int evntArg,
                            int evntArg2) {}
};
} // namespace

// V2-C1: services declare their handlers after the manager, so they are
// destroyed first. After Stop() no library thread may call them any more.
TEST_CASE("V2-C1: handler destroyed after Stop() while clients keep sending") {
  int port = test_port();
  for (int round = 0; round < 10; round++) {
    ADJsonRpcMgr *mgr = new ADJsonRpcMgr(1, false, NULL);
    SlowHandler *h = new SlowHandler;
    mgr->AttachRpc(h);
    mgr->AttachEventReceiver(h);
    REQUIRE(mgr->Start(port, 0, 0) == 0);
    const int N = 8;
    pthread_t th[N];
    LoadArg arg(port, "test_slow");
    for (int i = 0; i < N; i++)
      pthread_create(&th[i], NULL, load_client, &arg);
    usleep(50000);
    mgr->Stop();
    CHECK(__atomic_load_n(&h->calls, __ATOMIC_SEQ_CST) > 0);
    delete h; // what main() does when it returns
    usleep(20000);
    __atomic_store_n(&arg.stop, 1, __ATOMIC_SEQ_CST);
    for (int i = 0; i < N; i++)
      pthread_join(th[i], NULL);
    delete mgr;
  }
}

// V2-H1: Start() used to return 0 although nothing was listening
TEST_CASE("V2-H1: Start() on a busy port returns an error") {
  int lfd = socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(lfd >= 0);
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(test_port(1));
  REQUIRE(bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
  REQUIRE(listen(lfd, 4) == 0);
  ADJsonRpcMgr mgr(1, false, NULL);
  CHECK(mgr.Start(test_port(1), 0, 0) != 0);
  close(lfd);
}

// V2-H1: a child started by a handler used to inherit the listening socket,
// so the service could not bind its port again while the child lived
TEST_CASE("V2-H1: restart while a child process is still running") {
  int port = test_port(2);
  ADJsonRpcMgr *mgr = new ADJsonRpcMgr(1, false, NULL);
  REQUIRE(mgr->Start(port, 0, 0) == 0);
  REQUIRE(server_alive(port));
  // the child lives longer than the first server instance
  REQUIRE(system("sleep 3 &") == 0);
  delete mgr;
  mgr = new ADJsonRpcMgr(1, false, NULL);
  CHECK(mgr->Start(port, 0, 0) == 0);
  CHECK(server_alive(port));
  delete mgr;
}

TEST_CASE("bind address: listening on 127.0.0.1 only") {
  ADJsonRpcMgr mgr(1, false, NULL);
  CHECK(mgr.Start(test_port(3), 0, 0, "not-an-ip") != 0);
  ADJsonRpcMgr mgr2(1, false, NULL);
  REQUIRE(mgr2.Start(test_port(3), 0, 0, "127.0.0.1") == 0);
  CHECK(server_alive(test_port(3)));
}

TEST_CASE("H2: Stop() is idempotent and the destructor still works") {
  ADJsonRpcMgr *mgr = new ADJsonRpcMgr(1, false, NULL);
  REQUIRE(mgr->Start(test_port(), 0, 0) == 0);
  REQUIRE(server_alive(test_port()));
  mgr->Stop();
  mgr->Stop();
  CHECK(!server_alive(test_port(), 300));
  delete mgr;
}

namespace {
class Worker : public ADTaskWorkerConsumer, public ADTaskWorkerEventSink {
public:
  pthread_mutex_t m;
  std::vector<int> ran;
  std::vector<int> done_events;
  int sleep_us;
  Worker() : sleep_us(0) { pthread_mutex_init(&m, NULL); }
  virtual RPC_SRV_RESULT run_work(int cmd, unsigned char *pWorkData,
                                  ADTaskWorkerProducer *) {
    if (sleep_us)
      usleep(sleep_us);
    delete pWorkData; // the consumer owns the work data of tasks that run
    pthread_mutex_lock(&m);
    ran.push_back(cmd);
    pthread_mutex_unlock(&m);
    return cmd % 2 ? RPC_SRV_RESULT_FAIL : RPC_SRV_RESULT_SUCCESS;
  }
  virtual void task_worker_event(int evntNum, int evntArg, int evntArg2) {
    pthread_mutex_lock(&m);
    done_events.push_back(evntArg);
    pthread_mutex_unlock(&m);
  }
  size_t ran_count() {
    pthread_mutex_lock(&m);
    size_t n = ran.size();
    pthread_mutex_unlock(&m);
    return n;
  }
};
bool wait_ran(Worker &w, size_t n, int timeout_ms = 5000) {
  long end = now_ms() + timeout_ms;
  while (w.ran_count() < n && now_ms() < end)
    usleep(1000);
  return w.ran_count() >= n;
}
} // namespace

TEST_CASE("H3/H4: tasks run in order, done events in-process, status") {
  Worker w;
  ADTaskWorker tw;
  tw.attach_helper(&w);
  tw.set_event_sink(&w);
  std::vector<int> ids;
  for (int i = 0; i < 20; i++) {
    int id = -1;
    REQUIRE(tw.push_task(i, NULL, &id) == 0);
    ids.push_back(id);
  }
  REQUIRE(wait_ran(w, 20));
  usleep(10000);
  for (int i = 0; i < 20; i++)
    CHECK_EQ(w.ran[i], i);
  CHECK_EQ(w.done_events.size(), (size_t)20); // no TCP round trip needed
  int sts = -1;
  char msg[255];
  CHECK(tw.get_task_status(ids[0], &sts, msg) == RPC_SRV_RESULT_SUCCESS);
  CHECK_EQ(sts, (int)RPC_SRV_RESULT_SUCCESS);
  CHECK(tw.get_task_status(ids[1], &sts, msg) == RPC_SRV_RESULT_SUCCESS);
  CHECK_EQ(sts, (int)RPC_SRV_RESULT_FAIL);
  // a finished status is reported once, then the record is gone
  tw.get_task_status(ids[0], &sts, msg);
  CHECK_EQ(sts, (int)RPC_SRV_RESULT_TASK_ID_NOT_FOUND);
}

TEST_CASE("H4: status is in progress while running, reset while running") {
  Worker w;
  w.sleep_us = 100000;
  ADTaskWorker tw;
  tw.attach_helper(&w);
  tw.set_event_sink(&w);
  int id = -1;
  REQUIRE(tw.push_task(2, NULL, &id) == 0);
  int sts = -1;
  char msg[255];
  tw.get_task_status(id, &sts, msg);
  CHECK_EQ(sts, (int)RPC_SRV_RESULT_IN_PROG);
  CHECK(tw.reset_task_id_and_chain() == RPC_SRV_RESULT_SUCCESS);
  REQUIRE(wait_ran(w, 1));
  usleep(10000);
  tw.get_task_status(id, &sts, msg);
  CHECK_EQ(sts, (int)RPC_SRV_RESULT_TASK_ID_NOT_FOUND);
}

TEST_CASE("H4: unpolled finished tasks are capped") {
  Worker w;
  ADTaskWorker tw;
  tw.attach_helper(&w);
  tw.set_event_sink(&w);
  std::vector<int> ids;
  for (int i = 0; i < 300; i++) {
    int id = -1;
    REQUIRE(tw.push_task(0, NULL, &id) == 0);
    ids.push_back(id);
    if (i % 50 == 49)
      wait_ran(w, i + 1);
  }
  REQUIRE(wait_ran(w, 300));
  int sts = -1;
  char msg[255];
  tw.get_task_status(ids[0], &sts, msg); // oldest finished: evicted
  CHECK_EQ(sts, (int)RPC_SRV_RESULT_TASK_ID_NOT_FOUND);
  tw.get_task_status(ids[299], &sts, msg); // newest: still there
  CHECK_EQ(sts, (int)RPC_SRV_RESULT_SUCCESS);
}

TEST_CASE("H4: destroy the worker with pending tasks") {
  for (int round = 0; round < 50; round++) {
    Worker w;
    w.sleep_us = 2000;
    ADTaskWorker *tw = new ADTaskWorker;
    tw->attach_helper(&w);
    tw->set_event_sink(&w);
    for (int i = 0; i < 20; i++) {
      int id;
      // the worker deletes the work data of tasks that never ran
      unsigned char *data = new unsigned char(0);
      tw->push_task(i, data, &id);
      if (i == 0)
        usleep(100);
    }
    delete tw;
  }
}

ADTEST_MAIN()
