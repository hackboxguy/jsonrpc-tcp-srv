#include "ADTimer.hpp"
#include "ADCommon.hpp"
#include "ADJsonRpcClient.hpp"
#include <errno.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
using namespace std;
// stoptimer/received_user_stop_sig are written from signal handlers and
// other threads: always accessed with __atomic builtins (finding M3)
int ADTimer::received_user_stop_sig = 0;
int ADTimer::stoptimer = 0;
ADTimer *pTmpTimer;
// thread that waits in wait_for_exit_signal(); signals that the kernel
// delivers to any other thread are forwarded to it (finding H1)
static pid_t waiter_tid = 0;
static pid_t current_tid() { return (pid_t)syscall(SYS_gettid); }
// Process-directed signals can be delivered to any thread that does not
// block them. Worker threads keep their signals unblocked on purpose:
// processes they spawn (popen/system) inherit the signal mask, and blocked
// SIGTERM/SIGINT in those children would make them unkillable. Instead this
// handler re-queues the signal (with its siginfo payload) to the waiting
// thread, which blocks everything and consumes it in sigwaitinfo().
void ADTimer::forward_signal_handler(int sig, siginfo_t *info, void *context) {
  int saved_errno = errno;
  pid_t target = __atomic_load_n(&waiter_tid, __ATOMIC_SEQ_CST);
  if (target > 0 && current_tid() != target) {
    siginfo_t copy = *info;
    // the kernel only lets the thread group leader re-send kernel/kill()
    // generated codes; SI_QUEUE keeps the payload (si_int) for sigqueue()
    if (copy.si_code >= 0 || copy.si_code == SI_TKILL)
      copy.si_code = SI_QUEUE;
    if (syscall(SYS_rt_tgsigqueueinfo, getpid(), target, sig, &copy) != 0)
      syscall(SYS_tgkill, getpid(), target, sig);
  } else if (sig == SIGINT || sig == SIGTERM || sig == SIGQUIT) {
    // arrived on the waiting thread before it entered sigwaitinfo()
    __atomic_store_n(&received_user_stop_sig, 1, __ATOMIC_SEQ_CST);
  }
  errno = saved_errno;
}
int ADTimer::install_forwarder(int sig) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = &ADTimer::forward_signal_handler;
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigemptyset(&sa.sa_mask);
  return sigaction(sig, &sa, NULL);
}
ADTimer::ADTimer() : millisec_time(100), passive_mode(true) {
  custom_sig_chain.set_element_deleter(
      &chain_delete_object<ADTIMER_CUSTOM_SIG>);
  SigInfoChain.set_element_deleter(&chain_delete_object<ADTIMER_CUSTOM_SIG>);
}
ADTimer::ADTimer(int timer_millisec, int port) {
  notifyPortNum = port;
  passive_mode = false;
  __atomic_store_n(&waiter_tid, current_tid(), __ATOMIC_SEQ_CST);
  const int forwarded[] = {SIGINT, SIGTERM, SIGQUIT, SIGHUP, SIGALRM, SIGIO};
  for (size_t i = 0; i < sizeof(forwarded) / sizeof(forwarded[0]); i++)
    install_forwarder(forwarded[i]);
  custom_sig_chain.set_element_deleter(
      &chain_delete_object<ADTIMER_CUSTOM_SIG>);
  SigInfoChain.set_element_deleter(&chain_delete_object<ADTIMER_CUSTOM_SIG>);
  __atomic_store_n(&stoptimer, 0, __ATOMIC_SEQ_CST);
  pTmpTimer = this;
  __atomic_store_n(&received_user_stop_sig, 0, __ATOMIC_SEQ_CST);
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
    __atomic_store_n(&stoptimer, 1, __ATOMIC_SEQ_CST);
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
  __atomic_store_n(&received_user_stop_sig, 1, __ATOMIC_SEQ_CST);
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
  __atomic_store_n(&stoptimer, 1, __ATOMIC_SEQ_CST);
  return 0;
}
void ADTimer::millisec_signal_handler(int sig_no) {
  if (__atomic_load_n(&stoptimer, __ATOMIC_SEQ_CST) == 1)
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
  // synchronous fault signals cannot be waited for; keep them deliverable
  sigdelset(&sigset, SIGSEGV);
  sigdelset(&sigset, SIGBUS);
  sigdelset(&sigset, SIGFPE);
  sigdelset(&sigset, SIGILL);
  pthread_sigmask(SIG_BLOCK, &sigset, NULL);
  __atomic_store_n(&waiter_tid, current_tid(), __ATOMIC_SEQ_CST);
  setitimer(ITIMER_REAL, &timer, NULL);
  int sig;
  siginfo_t info;
  while (__atomic_load_n(&received_user_stop_sig, __ATOMIC_SEQ_CST) == 0) {
    sig = sigwaitinfo(&sigset, &info);
    if (sig < 0)
      continue; // EINTR
    switch (sig) {
    case SIGINT:
      LOG_INFO_MSG("SDSRV:AdLib", "ADTimer received SIGINT");
      __atomic_store_n(&received_user_stop_sig, 1, __ATOMIC_SEQ_CST);
      break;
    case SIGTERM:
      LOG_INFO_MSG("SDSRV:AdLib", "ADTimer received SIGTERM");
      __atomic_store_n(&received_user_stop_sig, 1, __ATOMIC_SEQ_CST);
      break;
    case SIGQUIT:
      LOG_INFO_MSG("SDSRV:AdLib", "ADTimer received SIGQUIT");
      __atomic_store_n(&received_user_stop_sig, 1, __ATOMIC_SEQ_CST);
      break;
    case SIGIO:
      notify_sigio_to_subscribers();
      break;
    case SIGALRM:
      if (__atomic_load_n(&stoptimer, __ATOMIC_SEQ_CST) != 1)
        TimerThread.wakeup_thread();
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
  if (passive_mode == false)
    install_forwarder(custom_sig_num);
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
  if (passive_mode == false)
    install_forwarder(custom_sig_num);
  push_custom_sig_registration(custom_sig_num);
  return 0;
}
void ADTimer::forced_exit() {
  __atomic_store_n(&received_user_stop_sig, 1, __ATOMIC_SEQ_CST);
  // wake the waiting thread now instead of relying on the next SIGALRM
  pid_t target = __atomic_load_n(&waiter_tid, __ATOMIC_SEQ_CST);
  if (target > 0 && target != current_tid())
    syscall(SYS_tgkill, getpid(), target, SIGALRM);
}
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
