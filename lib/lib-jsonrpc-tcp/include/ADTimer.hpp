#ifndef __ADTIMER_H_
#define __ADTIMER_H_
#include "ADGenericChain.hpp"
#include "ADThread.hpp"
#include <signal.h>
#include <string>
#include <sys/time.h>
#include <time.h>
#include <typeinfo>
#include <vector>
typedef struct ADTIMER_CUSTOM_SIG_T {
  int sig_num;
  int sig_extra;
} ADTIMER_CUSTOM_SIG;
class ADTimerProducer;
class ADTimerConsumer {
public:
  int notify_custom_sig;
  int custom_sig_num;
  virtual int timer_notification() = 0;
  virtual int sigio_notification() = 0;
  virtual int custom_sig_notification(int signum) = 0;
  virtual ~ADTimerConsumer(){};
};
class ADTimerProducer {
  std::vector<ADTimerConsumer *> subscribers;

protected:
  void notify_subscribers() {
    std::vector<ADTimerConsumer *>::iterator iter;
    for (iter = subscribers.begin(); iter != subscribers.end(); ++iter)
      (*iter)->timer_notification();
  }
  void notify_sigio_to_subscribers() {
    std::vector<ADTimerConsumer *>::iterator iter;
    for (iter = subscribers.begin(); iter != subscribers.end(); ++iter)
      (*iter)->sigio_notification();
  }
  void notify_custom_sig_to_subscribers(int signum) {
    std::vector<ADTimerConsumer *>::iterator iter;
    for (iter = subscribers.begin(); iter != subscribers.end(); ++iter) {
      if ((*iter)->notify_custom_sig == 1)
        (*iter)->custom_sig_notification(signum);
    }
  }
  void notify_custom_sig_to_subscribers_new(int signum, int sig_extra) {
    std::vector<ADTimerConsumer *>::iterator iter;
    for (iter = subscribers.begin(); iter != subscribers.end(); ++iter) {
      if ((*iter)->notify_custom_sig == 1)
        (*iter)->custom_sig_notification(signum);
    }
  }

public:
  virtual ~ADTimerProducer(){};
  void subscribe_timer_notification(ADTimerConsumer *pConsumer) {
    pConsumer->notify_custom_sig = 0;
    subscribers.push_back(pConsumer);
  }
};
class ADTimer : public ADTimerProducer, public ADThreadConsumer {
private:
  ADGenericChain custom_sig_chain;
  struct itimerval timer;
  sigset_t sigset;
  int notifyPortNum;
  static int stoptimer;
  int millisec_time;
  bool passive_mode;
  static int received_user_stop_sig;
  int prepare_to_stop(void);
  static void apptimer_stop_handler(int sig_no);
  static void millisec_signal_handler(int sig_no);
  int start_millisec_timer(int ms);
  static void custom_signal_handler(int sig, siginfo_t *info, void *context);
  static void custom_signal_handler_new(int sig, siginfo_t *info,
                                        void *context);
  int push_custom_sig_registration(int sig);
  int notify_registered_signals(int sig, siginfo_t *info);
  ADThread TimerThread, CustomSigThread;
  int TimerThreadID, CustomSigThreadID;
  ADGenericChain SigInfoChain;
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj);
  virtual int thread_callback_function(void *pUserData,
                                       ADThreadProducer *pObj) {
    return 0;
  };

public:
  ADTimer();
  ADTimer(int timer_millisec, int port = -1);
  ~ADTimer();
  int test_print();
  int restart_millisec_timer(int new_millisec);
  int wait_for_exit_signal();
  int stop_timer();
  int get_100ms_heartbeat();
  int get_sigio_event();
  int register_custom_signal(int custom_sig_num, ADTimerConsumer *pConsumer);
  int register_custom_signal_new(int custom_sig_num,
                                 ADTimerConsumer *pConsumer);
  void forced_exit();
};
#endif
