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

ADTEST_MAIN()
