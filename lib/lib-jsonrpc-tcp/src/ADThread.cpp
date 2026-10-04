#include "ADThread.hpp"
#include "ADCommon.hpp"
#include <errno.h>
#include <iostream>
#include <signal.h>
#include <time.h>
using namespace std;
int ADThreadProducer::IDGenerator = 0;
static void *thread_function(void *thread_attr) {
  ADThread *pThread;
  pThread = (ADThread *)thread_attr;
  // no signal() here: dispositions are process wide (review finding H1)
  pThread->my_thread_func(0);
  return NULL;
}
void ADThread::init_common() {
  started = false;
  finished = 1;
  stop_timeout_ms = ADTHREAD_STOP_TIMEOUT_MS;
  __atomic_store_n(&thread_state, (int)THREAD_STATE_INACTIVE, __ATOMIC_SEQ_CST);
  pthread_mutex_init(&ctrl_lock, NULL);
  if (sem_init(&one_shot_sema, 0, 0) != 0)
    cout << "unable to init semaphore" << endl;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  size_t stack_size = 0;
  if (pthread_attr_getstacksize(&attr, &stack_size) == 0 &&
      stack_size < ADTHREAD_MIN_STACK_SIZE)
    pthread_attr_setstacksize(&attr, ADTHREAD_MIN_STACK_SIZE);
}
ADThread::ADThread()
    : th_type(THREAD_TYPE_NONE), user_data(NULL), init_flag(false) {
  init_common();
}
ADThread::ADThread(THRD_TYPE type, void *usr_dat)
    : th_type(type), user_data(usr_dat), init_flag(true) {
  init_common();
}
ADThread::~ADThread() {
  stop_thread();
  pthread_attr_destroy(&attr);
  sem_destroy(&one_shot_sema);
  pthread_mutex_destroy(&ctrl_lock);
}
int ADThread::set_thread_properties(THRD_TYPE type, void *usr_dat) {
  th_type = type;
  user_data = usr_dat;
  init_flag = true;
  return 0;
}
int ADThread::test_print(void) {
  cout << "This is ADThread" << endl;
  return 0;
}
bool ADThread::stop_requested() {
  return __atomic_load_n(&thread_state, __ATOMIC_SEQ_CST) !=
         THREAD_STATE_ACTIVE;
}
bool ADThread::is_running() { return !stop_requested(); }
int ADThread::my_thread_func(int thread_id) {
  while (__atomic_load_n(&thread_state, __ATOMIC_SEQ_CST) ==
         THREAD_STATE_ACTIVE) {
    if (th_type == THREAD_TYPE_MONOSHOT) {
      if (sem_wait(&one_shot_sema) != 0)
        continue; // EINTR: no work was posted
      if (__atomic_load_n(&thread_state, __ATOMIC_SEQ_CST) !=
          THREAD_STATE_ACTIVE)
        break;
      if (is_user_callback_object_attached() == 0)
        user_monoshot_callback_function(user_data);
    } else {
      if (is_user_callback_object_attached() == 0)
        user_thread_callback_function(user_data);
      // the callback returned on its own: allow start_thread() again
      __atomic_store_n(&thread_state, (int)THREAD_STATE_INACTIVE,
                       __ATOMIC_SEQ_CST);
      break;
    }
  }
  // last access to this object from the thread: join_thread() waits for it
  __atomic_store_n(&finished, 1, __ATOMIC_SEQ_CST);
  return 0;
}
// joins the thread; cancels it only if it does not finish in time
// waits for the thread without ever cancelling it (see ADThread.hpp)
int ADThread::join_thread() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long start = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
  long next_log = start + (stop_timeout_ms > 0 ? stop_timeout_ms : 0);
  useconds_t pause_us = 50;
  while (!__atomic_load_n(&finished, __ATOMIC_SEQ_CST)) {
    usleep(pause_us);
    if (pause_us < 2000)
      pause_us *= 2;
    if (stop_timeout_ms <= 0)
      continue;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long now = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
    if (now >= next_log) {
      LOG_ERR_MSG_WITH_ARG("libadav:ADThread",
                           "thread still busy %ld ms after stop request, "
                           "waiting for it",
                           now - start);
      next_log = now + stop_timeout_ms;
    }
  }
  void *status;
  int rc = pthread_join(thread, &status); // returns at once now
  if (rc != 0)
    cout << "unable to stop the thread" << endl;
  return rc;
}
int ADThread::start_thread(void) {
  if (init_flag == false)
    return -1;
  pthread_mutex_lock(&ctrl_lock);
  if (started) {
    if (__atomic_load_n(&thread_state, __ATOMIC_SEQ_CST) ==
        THREAD_STATE_ACTIVE) {
      pthread_mutex_unlock(&ctrl_lock);
      return -1; // already running
    }
    join_thread(); // a NOBLOCK callback that already returned
    started = false;
  }
  // drop wakeups left over from a previous run
  while (sem_trywait(&one_shot_sema) == 0)
    ;
  __atomic_store_n(&thread_state, (int)THREAD_STATE_ACTIVE, __ATOMIC_SEQ_CST);
  __atomic_store_n(&finished, 0, __ATOMIC_SEQ_CST);
  if (pthread_create(&thread, &attr, thread_function, (void *)this) != 0) {
    __atomic_store_n(&finished, 1, __ATOMIC_SEQ_CST);
    __atomic_store_n(&thread_state, (int)THREAD_STATE_INACTIVE,
                     __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&ctrl_lock);
    cout << "thread could not be started" << endl;
    return -1;
  }
  started = true;
  pthread_mutex_unlock(&ctrl_lock);
  return 0;
}
int ADThread::stop_thread() {
  pthread_mutex_lock(&ctrl_lock);
  if (!started) {
    pthread_mutex_unlock(&ctrl_lock);
    return -1;
  }
  if (pthread_equal(thread, pthread_self())) {
    // stop requested from inside the callback: cannot join ourselves
    __atomic_store_n(&thread_state, (int)THREAD_STATE_INACTIVE,
                     __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&ctrl_lock);
    return 0;
  }
  __atomic_store_n(&thread_state, (int)THREAD_STATE_INACTIVE, __ATOMIC_SEQ_CST);
  if (th_type == THREAD_TYPE_MONOSHOT)
    sem_post(&one_shot_sema);
  join_thread();
  started = false;
  pthread_mutex_unlock(&ctrl_lock);
  return 0;
}
int ADThread::wakeup_thread(void) {
  if (th_type == THREAD_TYPE_MONOSHOT)
    sem_post(&one_shot_sema);
  return 0;
}
