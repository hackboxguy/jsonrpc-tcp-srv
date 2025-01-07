#include "ADTimer.hpp"
#include "ADCommon.hpp"
#include "ADJsonRpcClient.hpp"
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
using namespace std;
int ADTimer::received_user_stop_sig = 0;
int ADTimer::stoptimer = 0;
ADTimer *pTmpTimer;
ADTimer::ADTimer() : millisec_time(100), passive_mode(true) {}
ADTimer::ADTimer(int timer_millisec, int port) {
  notifyPortNum = port;
  passive_mode = false;
  stoptimer = 0;
  pTmpTimer = this;
  received_user_stop_sig = 0;
  millisec_time = timer_millisec;
  prepare_to_stop();
  start_millisec_timer(millisec_time);
  sigemptyset(&sigset);
  pthread_sigmask(SIG_SETMASK, &sigset, NULL);
  TimerThreadID = TimerThread.subscribe_thread_callback(this);
  TimerThread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  TimerThread.start_thread();
  CustomSigThreadID = CustomSigThread.subscribe_thread_callback(this);
  CustomSigThread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  CustomSigThread.start_thread();
}
ADTimer::~ADTimer() {
  TimerThread.stop_thread();
  CustomSigThread.stop_thread();
  if (passive_mode == false)
    stoptimer = 1;
  custom_sig_chain.remove_all();
  SigInfoChain.remove_all();
}
int ADTimer::test_print(void) {
  cout << "This is ADTimer" << endl;
  return 0;
}
int ADTimer::restart_millisec_timer(int new_millisec) {
  if (passive_mode == true)
    return 0;
  millisec_time = new_millisec;
  start_millisec_timer(millisec_time);
  return 0;
}
void ADTimer::apptimer_stop_handler(int sig_no) {
  switch (sig_no) {
  case SIGINT:
    cout << "got SIGINT" << endl;
    break;
  case SIGTERM:
    cout << "got SIGTERM" << endl;
    break;
  case SIGQUIT:
    cout << "got SIGQUIT" << endl;
    break;
  case SIGIO:
    pTmpTimer->notify_sigio_to_subscribers();
    return;
  default:
    return;
  }
  received_user_stop_sig = 1;
}
int ADTimer::prepare_to_stop(void) { return 0; }
int ADTimer::get_100ms_heartbeat() {
  if (passive_mode == true)
    notify_subscribers();
  return 0;
}
int ADTimer::get_sigio_event() {
  if (passive_mode == true)
    notify_sigio_to_subscribers();
  return 0;
}
int ADTimer::stop_timer() {
  if (passive_mode == true)
    return 0;
  stoptimer = 1;
  return 0;
}
void ADTimer::millisec_signal_handler(int sig_no) {
  if (stoptimer == 1)
    return;
  pTmpTimer->notify_subscribers();
}
int ADTimer::start_millisec_timer(int ms) {
  if (passive_mode == true)
    return 0;
  if (ms < 1000) {
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = (ms * 1000);
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = (ms * 1000);
  } else {
    timer.it_value.tv_sec = (ms / 1000);
    timer.it_value.tv_usec = (ms % 1000) * 1000;
    timer.it_interval.tv_sec = (ms / 1000);
    timer.it_interval.tv_usec = (ms % 1000) * 1000;
  }
  return 0;
}
int ADTimer::wait_for_exit_signal() {
  if (passive_mode == true)
    return 0;
  sigemptyset(&sigset);
  sigfillset(&sigset);
  pthread_sigmask(SIG_BLOCK, &sigset, NULL);
  setitimer(ITIMER_REAL, &timer, NULL);
  int result, sig;
  siginfo_t info;
  while (received_user_stop_sig == 0) {
    sig = sigwaitinfo(&sigset, &info);
    switch (sig) {
    case SIGINT:
      LOG_INFO_MSG("SDSRV:AdLib", "ADTimer received SIGINT");
      received_user_stop_sig = 1;
      break;
    case SIGTERM:
      LOG_INFO_MSG("SDSRV:AdLib", "ADTimer received SIGTERM");
      received_user_stop_sig = 1;
      break;
    case SIGQUIT:
      LOG_INFO_MSG("SDSRV:AdLib", "ADTimer received SIGQUIT");
      received_user_stop_sig = 1;
      break;
    case SIGIO:
      notify_sigio_to_subscribers();
      break;
    case SIGALRM:
      if (stoptimer != 1)
        TimerThread.wakeup_thread();
      break;
    case SIGSEGV:
      LOG_INFO_MSG("SDSRV:AdLib",
                   "ADTimer received Sementation fault!!!!!!!!!!!!!!!!!!!!!!");
      break;
    default:
      if (notify_registered_signals(sig, &info) != 0)
        ;
      break;
    }
  }
  if (notifyPortNum != -1) {
    NOTIFY_EVENT(ADLIB_EVENT_NUM_SHUT_DOWN, -1, notifyPortNum, -1);
    usleep(100000);
    usleep(100000);
    usleep(100000);
    usleep(100000);
    usleep(100000);
  }
  return 0;
}
void ADTimer::custom_signal_handler(int sig, siginfo_t *info, void *context) {
  pTmpTimer->notify_custom_sig_to_subscribers(sig);
}
int ADTimer::register_custom_signal(int custom_sig_num,
                                    ADTimerConsumer *pConsumer) {
  pConsumer->notify_custom_sig = 1;
  pConsumer->custom_sig_num = custom_sig_num;
  sigaddset(&sigset, custom_sig_num);
  pthread_sigmask(SIG_SETMASK, &sigset, NULL);
  push_custom_sig_registration(custom_sig_num);
  return 0;
}
void ADTimer::custom_signal_handler_new(int sig, siginfo_t *info,
                                        void *context) {
  pTmpTimer->notify_custom_sig_to_subscribers_new(info->si_int, sig);
}
int ADTimer::register_custom_signal_new(int custom_sig_num,
                                        ADTimerConsumer *pConsumer) {
  pConsumer->notify_custom_sig = 1;
  pConsumer->custom_sig_num = custom_sig_num;
  sigaddset(&sigset, custom_sig_num);
  pthread_sigmask(SIG_SETMASK, &sigset, NULL);
  push_custom_sig_registration(custom_sig_num);
  return 0;
}
void ADTimer::forced_exit() { received_user_stop_sig = 1; }
int ADTimer::push_custom_sig_registration(int sig) {
  ADTIMER_CUSTOM_SIG *pSigReg = NULL;
  OBJECT_MEM_NEW(pSigReg, ADTIMER_CUSTOM_SIG);
  if (pSigReg == NULL) {
    LOG_ERR_MSG("SDSRV:ADTimer",
                "unable create custom sig registration object");
    return -1;
  }
  pSigReg->sig_num = sig;
  if (custom_sig_chain.chain_put((void *)pSigReg) != 0) {
    OBJ_MEM_DELETE(pSigReg);
    LOG_ERR_MSG("SDSRV:ADTimer", "unable to push custom sig registration");
    return -1;
  }
  return 0;
}
int ADTimer::notify_registered_signals(int sig, siginfo_t *info) {
  int result = -1;
  ADTIMER_CUSTOM_SIG *pSigReg = NULL;
  int chain_size = custom_sig_chain.get_chain_size();
  custom_sig_chain.chain_lock();
  for (int i = 0; i < chain_size; i++) {
    pSigReg = (ADTIMER_CUSTOM_SIG *)custom_sig_chain.chain_get_by_index(i);
    if (pSigReg != NULL) {
      if (sig == pSigReg->sig_num) {
        ADTIMER_CUSTOM_SIG *pSigInfo = NULL;
        OBJECT_MEM_NEW(pSigInfo, ADTIMER_CUSTOM_SIG);
        if (pSigInfo != NULL) {
          pSigInfo->sig_num = info->si_int;
          pSigInfo->sig_extra = sig;
          if (SigInfoChain.chain_put((void *)pSigInfo) != 0)
            OBJ_MEM_DELETE(pSigInfo);
          else
            CustomSigThread.wakeup_thread();
        }
        result = 0;
      }
    }
  }
  custom_sig_chain.chain_unlock();
  return result;
}
int ADTimer::monoshot_callback_function(void *pUserData,
                                        ADThreadProducer *pObj) {
  int call_from = pObj->getID();
  if (call_from == TimerThreadID) {
    pTmpTimer->notify_subscribers();
  } else if (call_from == CustomSigThreadID) {
    ADTIMER_CUSTOM_SIG *pSigReg = NULL;
    pSigReg = (ADTIMER_CUSTOM_SIG *)SigInfoChain.chain_get();
    if (pSigReg != NULL) {
      notify_custom_sig_to_subscribers_new(pSigReg->sig_num,
                                           pSigReg->sig_extra);
      OBJ_MEM_DELETE(pSigReg);
    }
  }
  return 0;
}
