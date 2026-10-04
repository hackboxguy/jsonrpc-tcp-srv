// ADThread lifecycle tests (review finding C6). Run under ASan and TSan.
#include "ADThread.hpp"
#include "adtest.hpp"
#include <pthread.h>
#include <time.h>

namespace {
long now_ms() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}
class Counter : public ADThreadConsumer {
public:
  int mono_calls;
  int loop_calls;
  pthread_mutex_t *hold_lock; // locked during each monoshot callback
  ADThread *self;             // for NOBLOCK: poll stop_requested()
  int sleep_us;
  Counter()
      : mono_calls(0), loop_calls(0), hold_lock(NULL), self(NULL), sleep_us(0) {
  }
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj) {
    if (hold_lock)
      pthread_mutex_lock(hold_lock);
    if (sleep_us)
      usleep(sleep_us);
    __atomic_add_fetch(&mono_calls, 1, __ATOMIC_SEQ_CST);
    if (hold_lock)
      pthread_mutex_unlock(hold_lock);
    return 0;
  }
  virtual int thread_callback_function(void *pUserData,
                                       ADThreadProducer *pObj) {
    __atomic_add_fetch(&loop_calls, 1, __ATOMIC_SEQ_CST);
    while (self != NULL && !self->stop_requested())
      usleep(1000);
    return 0;
  }
  int mono() { return __atomic_load_n(&mono_calls, __ATOMIC_SEQ_CST); }
  int loops() { return __atomic_load_n(&loop_calls, __ATOMIC_SEQ_CST); }
};
bool wait_for(Counter &c, int mono, int timeout_ms = 2000) {
  long end = now_ms() + timeout_ms;
  while (c.mono() < mono && now_ms() < end)
    usleep(1000);
  return c.mono() >= mono;
}
} // namespace

TEST_CASE("thread: monoshot runs one callback per wakeup") {
  Counter c;
  ADThread t;
  t.subscribe_thread_callback(&c);
  t.set_thread_properties(THREAD_TYPE_MONOSHOT, NULL);
  REQUIRE(t.start_thread() == 0);
  CHECK(t.start_thread() == -1); // already running
  for (int i = 0; i < 100; i++)
    t.wakeup_thread();
  CHECK(wait_for(c, 100));
  usleep(20000);
  CHECK_EQ(c.mono(), 100);
  CHECK(t.stop_thread() == 0);
  CHECK(t.stop_thread() == -1); // idempotent
}

TEST_CASE("thread: start/stop/start") {
  Counter c;
  ADThread t;
  t.subscribe_thread_callback(&c);
  t.set_thread_properties(THREAD_TYPE_MONOSHOT, NULL);
  for (int round = 1; round <= 5; round++) {
    REQUIRE(t.start_thread() == 0);
    t.wakeup_thread();
    CHECK(wait_for(c, round));
    REQUIRE(t.stop_thread() == 0);
  }
  CHECK_EQ(c.mono(), 5);
}

// C6(b): stop before the new thread was scheduled used to skip the join
TEST_CASE("thread: stop right after start, 10k times") {
  Counter c;
  for (int i = 0; i < 10000; i++) {
    ADThread *t = new ADThread(THREAD_TYPE_MONOSHOT, NULL);
    t->subscribe_thread_callback(&c);
    REQUIRE(t->start_thread() == 0);
    if (i % 2)
      t->wakeup_thread();
    t->stop_thread();
    delete t; // must not be touched by the thread after this
  }
}

TEST_CASE("thread: destroy without explicit stop") {
  Counter c;
  for (int i = 0; i < 1000; i++) {
    ADThread *t = new ADThread(THREAD_TYPE_MONOSHOT, NULL);
    t->subscribe_thread_callback(&c);
    t->start_thread();
    t->wakeup_thread();
    delete t;
  }
}

// C6(c): the old pthread_cancel could hit a callback holding a mutex and
// leave it locked forever
TEST_CASE("thread: stop while a callback holds a lock does not deadlock") {
  pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
  Counter c;
  c.hold_lock = &m;
  c.sleep_us = 50000;
  ADThread t(THREAD_TYPE_MONOSHOT, NULL);
  t.subscribe_thread_callback(&c);
  REQUIRE(t.start_thread() == 0);
  t.wakeup_thread();
  usleep(10000); // the callback now sleeps holding m
  long t0 = now_ms();
  t.stop_thread();
  CHECK(now_ms() - t0 < 1000);
  CHECK(pthread_mutex_trylock(&m) == 0); // lock was released
  pthread_mutex_unlock(&m);
  CHECK_EQ(c.mono(), 1);
}

// C6(d): NOBLOCK threads that returned could never be started again
TEST_CASE("thread: NOBLOCK callback returns, thread can be restarted") {
  Counter c;
  ADThread t(THREAD_TYPE_NOBLOCK, NULL);
  t.subscribe_thread_callback(&c);
  for (int i = 1; i <= 3; i++) {
    REQUIRE(t.start_thread() == 0);
    long end = now_ms() + 2000;
    while (c.loops() < i && now_ms() < end)
      usleep(1000);
    CHECK_EQ(c.loops(), i);
    usleep(5000);
    CHECK(!t.is_running());
  }
}

TEST_CASE("thread: NOBLOCK cooperative stop via stop_requested()") {
  Counter c;
  ADThread t(THREAD_TYPE_NOBLOCK, NULL);
  c.self = &t;
  t.subscribe_thread_callback(&c);
  REQUIRE(t.start_thread() == 0);
  usleep(20000);
  CHECK(t.is_running());
  long t0 = now_ms();
  CHECK(t.stop_thread() == 0);
  CHECK(now_ms() - t0 < 500);
  CHECK(!t.is_running());
}

namespace {
class LongCallback : public ADThreadConsumer {
public:
  int done;
  int sleep_s;
  LongCallback(int s) : done(0), sleep_s(s) {}
  virtual int monoshot_callback_function(void *, ADThreadProducer *) {
    sleep(sleep_s); // e.g. a firmware write that must not be interrupted
    __atomic_store_n(&done, 1, __ATOMIC_SEQ_CST);
    return 0;
  }
  virtual int thread_callback_function(void *, ADThreadProducer *) { return 0; }
};
} // namespace

// V2-H2: the old 5 s stop timeout cancelled such a callback half way
TEST_CASE("V2-H2: a long callback is waited for, never cancelled") {
  LongCallback c(6);
  ADThread t(THREAD_TYPE_MONOSHOT, NULL);
  t.subscribe_thread_callback(&c);
  t.set_stop_timeout(1000); // logs every second while waiting
  REQUIRE(t.start_thread() == 0);
  t.wakeup_thread();
  usleep(100000);
  long t0 = now_ms();
  CHECK(t.stop_thread() == 0);
  CHECK(now_ms() - t0 >= 5000);
  CHECK_EQ(__atomic_load_n(&c.done, __ATOMIC_SEQ_CST), 1);
}

ADTEST_MAIN()
