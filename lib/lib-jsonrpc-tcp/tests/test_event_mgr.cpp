// ADEvntMgr concurrency (review finding C5): subscribe, unsubscribe and
// notify from several threads. Subscribers point at a closed port, so
// every send fails fast and removes the subscriber again.
#include "ADEvntMgr.hpp"
#include "adtest.hpp"
#include "test_net_util.hpp"
#include <pthread.h>

namespace {
struct Arg {
  ADEvntMgr *mgr;
  int id;
  int loops;
};
void *subscriber(void *p) {
  Arg *a = (Arg *)p;
  for (int i = 0; i < a->loops; i++) {
    EventEntry *e = new EventEntry;
    memset(e, 0, sizeof(*e));
    e->cltToken = a->id * 100000 + i;
    e->portNum = testnet::test_port(3); // nobody listens here
    e->eventNum = i % 3 == 0 ? -1 : i % 5;
    strcpy(e->ip, "127.0.0.1");
    int token = 0;
    if (a->mgr->register_event_subscription(e, &token) != 0)
      delete e;
    else if (i % 2)
      a->mgr->unregister_event_subscription(token);
  }
  return NULL;
}
void *notifier(void *p) {
  Arg *a = (Arg *)p;
  for (int i = 0; i < a->loops; i++) {
    a->mgr->notify_event(i % 5, i, -1);
    a->mgr->process_event(i % 5, i, a->id, -1);
    if (i % 64 == 0)
      usleep(1000);
  }
  return NULL;
}
class Receiver : public ADEvntMgrConsumer {
public:
  int received;
  Receiver() : received(0) {}
  virtual int receive_events(int cltToken, int evntNum, int evntArg,
                             int evntArg2) {
    __atomic_add_fetch(&received, 1, __ATOMIC_SEQ_CST);
    return 0;
  }
};
} // namespace

TEST_CASE("C5: concurrent subscribe/unsubscribe/notify") {
  Receiver r;
  {
    ADEvntMgr mgr;
    mgr.AttachReceiver(&r);
    pthread_t th[4];
    Arg args[4];
    for (int i = 0; i < 4; i++) {
      args[i].mgr = &mgr;
      args[i].id = i + 1;
      args[i].loops = 2000;
      pthread_create(&th[i], NULL, i < 2 ? subscriber : notifier, &args[i]);
    }
    for (int i = 0; i < 4; i++)
      pthread_join(th[i], NULL);
    // let the worker threads drain what is queued
    long end = testnet::now_ms() + 10000;
    while (__atomic_load_n(&r.received, __ATOMIC_SEQ_CST) < 4000 &&
           testnet::now_ms() < end)
      usleep(10000);
    CHECK_EQ(__atomic_load_n(&r.received, __ATOMIC_SEQ_CST), 4000);
  } // destructor stops threads and frees the remaining subscribers
}

namespace {
EventEntry *subscriber_entry(int port, int token) {
  EventEntry *e = new EventEntry;
  memset(e, 0, sizeof(*e));
  e->cltToken = token;
  e->portNum = port;
  e->eventNum = -1; // all events
  strcpy(e->ip, "127.0.0.1");
  return e;
}
std::string answer_success(const std::string &, int id, int) {
  return testnet::result_reply(id, "ok");
}
std::string never_answer(const std::string &, int, int) {
  return std::string();
}
int requests_of(testnet::FakeServer &f) {
  pthread_mutex_lock(&f.m);
  int n = f.requests;
  pthread_mutex_unlock(&f.m);
  return n;
}
bool wait_requests(testnet::FakeServer &f, int n, int timeout_ms) {
  long end = testnet::now_ms() + timeout_ms;
  while (requests_of(f) < n && testnet::now_ms() < end)
    usleep(10000);
  return requests_of(f) >= n;
}
} // namespace

// too slow for the 1 s delivery timeout for the first 6 events
static std::string slow_then_fast(const std::string &, int id, int n) {
  if (n <= 6)
    usleep(1500000);
  return testnet::result_reply(id, "ok");
}

// V3-M2: five quick failures used to remove a busy subscriber
TEST_CASE("V3-M2: a subscriber that is busy for a while stays subscribed") {
  int port = testnet::test_port(3);
  testnet::FakeServer sub(port, slow_then_fast);
  ADEvntMgr mgr;
  int token = 0;
  REQUIRE(mgr.register_event_subscription(subscriber_entry(port, 1), &token) ==
          0);
  for (int i = 0; i < 7; i++) {
    mgr.notify_event(i, 0, 0);
    // one event at a time, so each delivery is attempted
    CHECK(wait_requests(sub, i + 1, 5000));
  }
  CHECK(wait_requests(sub, 7, 5000)); // the 7th event still arrives
}

// V2-H4: one failed delivery used to remove the subscriber for good
TEST_CASE("V2-H4: a subscriber that is down for one event stays subscribed") {
  int port = testnet::test_port(1);
  ADEvntMgr mgr;
  int token = 0;
  REQUIRE(mgr.register_event_subscription(subscriber_entry(port, 1), &token) ==
          0);
  mgr.notify_event(7, 0, 0); // nobody listens yet: delivery fails
  usleep(300000);
  testnet::FakeServer sub(port, answer_success);
  mgr.notify_event(8, 0, 0);
  CHECK(wait_requests(sub, 1, 3000));
}

// V2-H4: a subscriber that accepts but never answers used to cost 4 s per
// event and delay every other subscriber
TEST_CASE("V2-H4: a black-hole subscriber does not hold up the others") {
  testnet::FakeServer hole(testnet::test_port(2), never_answer);
  testnet::FakeServer good(testnet::test_port(1), answer_success);
  ADEvntMgr mgr;
  int t1 = 0, t2 = 0;
  REQUIRE(mgr.register_event_subscription(
              subscriber_entry(testnet::test_port(2), 1), &t1) == 0);
  REQUIRE(mgr.register_event_subscription(
              subscriber_entry(testnet::test_port(1), 2), &t2) == 0);
  long t0 = testnet::now_ms();
  for (int i = 0; i < 3; i++)
    mgr.notify_event(i, 0, 0);
  CHECK(wait_requests(good, 3, 8000));
  CHECK(testnet::now_ms() - t0 < 5000); // 3 x ~1 s, was 3 x 4 s
}

ADTEST_MAIN()
