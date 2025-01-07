#ifndef __ADEVNT_NOTIFIER_H_
#define __ADEVNT_NOTIFIER_H_
#include "ADThread.hpp"
#include <deque>
#include <iostream>
#include <vector>
using namespace std;
struct EvntNotifyEntry {
  int eventNum;
  int eventArg;
  int port;
  int eventArg2;

public:
  EvntNotifyEntry(int event_num, int event_arg, int self_port, int event_arg2)
      : eventNum(event_num), eventArg(event_arg), port(self_port),
        eventArg2(event_arg2) {}
};
class ADEvntNotifier : public ADThreadConsumer {
  std::deque<EvntNotifyEntry> NotifierList;
  ADThread NotifierThread;
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj);
  virtual int thread_callback_function(void *pUserData,
                                       ADThreadProducer *pObj) {
    return 0;
  };

public:
  ADEvntNotifier();
  ~ADEvntNotifier();
  int NotifyEvent(int event, int eventArg, int port, int eventArg2);
};
#endif
