#include "ADThread.hpp"
#include <iostream>
#include <signal.h>
using namespace std;
int ADThreadProducer::IDGenerator = 0;
void *thread_function(void *thread_attr) {
  ADThread *pThread;
  pThread = (ADThread *)thread_attr;
  signal(SIGALRM, SIG_IGN);
  int my_id = pthread_self();
  pThread->my_thread_func(my_id);
  pthread_exit((void *)thread_attr);
  return NULL;
}
ADThread::ADThread()
    : th_type(THREAD_TYPE_NONE), user_data(NULL), init_flag(false) {
  if (sem_init(&one_shot_sema, 0, 0) != 0) {
    return;
  }
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  thread_state = THREAD_STATE_INACTIVE;
  tid = -1;
}
ADThread::ADThread(THRD_TYPE type, void *usr_dat) {
  th_type = type;
  user_data = usr_dat;
  if (sem_init(&one_shot_sema, 0, 0) != 0) {
    cout << "unable to init semaphore" << endl;
    return;
  }
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  thread_state = THREAD_STATE_INACTIVE;
  tid = -1;
  init_flag = true;
}
ADThread::~ADThread() { stop_thread(); }
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
int ADThread::my_thread_func(int thread_id) {
  tid = thread_id;
  while (thread_state == THREAD_STATE_ACTIVE) {
    if (th_type == THREAD_TYPE_MONOSHOT) {
      sem_wait(&one_shot_sema);
      if (is_user_callback_object_attached() == 0)
        user_monoshot_callback_function(user_data);
    } else {
      if (is_user_callback_object_attached() == 0)
        user_thread_callback_function(user_data);
      break;
    }
  }
  return 0;
}
int ADThread::start_thread(void) {
  if (thread_state == THREAD_STATE_ACTIVE)
    return -1;
  if (init_flag == false)
    return -1;
  thread_state = THREAD_STATE_ACTIVE;
  if (pthread_create(&thread, &attr, thread_function, (void *)this) != 0) {
    cout << "thread could not be started" << endl;
    return -1;
  }
  return 0;
}
int ADThread::stop_thread() {
  void *status;
  if (thread_state == THREAD_STATE_INACTIVE)
    return -1;
  thread_state = THREAD_STATE_INACTIVE;
  if (th_type == THREAD_TYPE_MONOSHOT)
    sem_post(&one_shot_sema);
  if (pthread_attr_destroy(&attr) != 0)
    cout << "unable to destroy thread attribute" << endl;
  if (tid != -1) {
    pthread_cancel(thread);
    if (pthread_join(thread, &status) != 0)
      cout << "unable to stop the thread" << endl;
  }
  if (sem_destroy(&one_shot_sema) != 0)
    cout << "unable to destroy semaphore!!!!!!" << endl;
  return 0;
}
int ADThread::wakeup_thread(void) {
  if (th_type == THREAD_TYPE_MONOSHOT)
    sem_post(&one_shot_sema);
  return 0;
}
