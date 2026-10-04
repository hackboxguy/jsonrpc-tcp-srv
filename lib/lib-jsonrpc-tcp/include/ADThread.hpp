#ifndef __ADTHREAD_H_
#define __ADTHREAD_H_
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
class ADThreadProducer;
class ADThreadConsumer {
public:
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj) = 0;
  virtual int thread_callback_function(void *pUserData,
                                       ADThreadProducer *pObj) = 0;
  virtual ~ADThreadConsumer(){};
};
class ADThreadProducer {
  static int IDGenerator;
  ADThreadConsumer *pConsumer;
  int id;

protected:
  int user_monoshot_callback_function(void *pUserData) {
    if (pConsumer != NULL)
      return pConsumer->monoshot_callback_function(pUserData, this);
    return -1;
  }
  int user_thread_callback_function(void *pUserData) {
    if (pConsumer != NULL)
      return pConsumer->thread_callback_function(pUserData, this);
    return -1;
  }
  int is_user_callback_object_attached(void) {
    if (pConsumer == NULL)
      return -1;
    return 0;
  }

public:
  ADThreadProducer() {
    id = IDGenerator++;
    pConsumer = NULL;
  }
  virtual ~ADThreadProducer(){};
  int subscribe_thread_callback(ADThreadConsumer *c) {
    if (pConsumer == NULL) {
      pConsumer = c;
      return id;
    } else
      return -1;
  }
  int getID() { return id; }
};
typedef enum THRD_STATE_T {
  THREAD_STATE_INACTIVE,
  THREAD_STATE_ACTIVE,
  THREAD_STATE_NONE
} THRD_STATE;
typedef enum THRD_TYPE_T {
  THREAD_TYPE_MONOSHOT,
  THREAD_TYPE_NOBLOCK,
  THREAD_TYPE_NONE
} THRD_TYPE;
// Stopping is cooperative (review finding C6):
// - MONOSHOT: stop_thread() marks the thread inactive, posts the semaphore
//   and joins. A callback that is running finishes first.
// - NOBLOCK: stop_thread() marks the thread inactive and joins. Long running
//   callbacks must poll stop_requested() (or their own flag) and return.
// - Only if the join does not finish within ADTHREAD_STOP_TIMEOUT_MS the
//   thread is cancelled as a last resort (logged loudly).
// A thread can be started again after stop_thread() or after its NOBLOCK
// callback returned.
#define ADTHREAD_STOP_TIMEOUT_MS 5000
// minimum stack size; musl's default (128 KB) is too small for the reply
// builders and json-c recursion. Larger platform defaults are kept.
#define ADTHREAD_MIN_STACK_SIZE (256 * 1024)
class ADThread : public ADThreadProducer {
  THRD_TYPE th_type;
  void *user_data;
  bool init_flag;
  int thread_state; // THRD_STATE, accessed with __atomic builtins
  bool started;     // a pthread exists that has not been joined yet
  pthread_t thread;
  pthread_attr_t attr;
  sem_t one_shot_sema;
  pthread_mutex_t ctrl_lock; // serializes start_thread/stop_thread
  void init_common();
  int join_thread();

public:
  ADThread();
  ADThread(THRD_TYPE type, void *usr_dat);
  ~ADThread();
  int set_thread_properties(THRD_TYPE type, void *usr_dat);
  int start_thread(void);
  int test_print();
  int my_thread_func(int thread_id);
  int stop_thread();
  int wakeup_thread(void);
  // true once stop_thread() was called; for NOBLOCK callbacks
  bool stop_requested();
  bool is_running();
};
#endif
