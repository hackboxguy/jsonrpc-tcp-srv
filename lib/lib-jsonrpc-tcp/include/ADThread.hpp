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
class ADThread : public ADThreadProducer {
  THRD_TYPE th_type;
  void *user_data;
  bool init_flag;
  THRD_STATE thread_state;
  int tid;
  pthread_t thread;
  pthread_attr_t attr;
  sem_t one_shot_sema;

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
};
#endif
