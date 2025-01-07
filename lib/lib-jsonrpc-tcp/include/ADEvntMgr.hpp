#ifndef __ADEVNT_MGR_H_
#define __ADEVNT_MGR_H_
#include "ADThread.hpp"
#include <deque>
#include <iostream>
#include <vector>
using namespace std;
typedef struct EventEntry_t {
  int cltToken;
  int portNum;
  int eventNum;
  int srvToken;
  int sock_descr;
  int sock_id;
  char ip[512];
  bool deleteFlag;
} EventEntry;
struct EventProcEntry {
  int eventNum;
  int eventArg;
  int cltToken;
  int eventArg2;

public:
  EventProcEntry(int event_num, int event_arg, int clt_token, int evnt_arg2)
      : eventNum(event_num), eventArg(event_arg), cltToken(clt_token),
        eventArg2(evnt_arg2) {}
};
class PrintEventEntries {
public:
  void operator()(EventEntry *Entry) const {
    cout << "cltToken=" << Entry->cltToken << " portNum=" << Entry->portNum
         << " eventNum=" << Entry->eventNum << " srvToken=" << Entry->srvToken;
    cout << " sock_descr=" << Entry->sock_descr << " sock_id=" << Entry->sock_id
         << " ip=" << Entry->ip << endl;
  }
};
class RemoveEventEntry {
  const int srv_token;

public:
  RemoveEventEntry(const int token) : srv_token(token) {}
  bool operator()(EventEntry *pEntry) {
    if (srv_token == pEntry->srvToken) {
      delete pEntry;
      pEntry = NULL;
      return true;
    }
    return false;
  }
};
class RemoveNonListenerEntry {
public:
  bool operator()(EventEntry *pEntry) {
    if (pEntry->deleteFlag == true) {
      delete pEntry;
      pEntry = NULL;
      return true;
    }
    return false;
  }
};
class FindEventEntry {
  const int srv_token;

public:
  FindEventEntry(const int token) : srv_token(token) {}
  bool operator()(EventEntry *pEntry) const {
    if (srv_token == pEntry->srvToken)
      return true;
    else
      return false;
  }
};
class FindDuplicateEntry {
  EventEntry *myEntry;

public:
  FindDuplicateEntry(EventEntry *entry) : myEntry(entry) {}
  bool operator()(EventEntry *pEntry) const {
    if (myEntry->cltToken != pEntry->cltToken)
      return false;
    if (myEntry->portNum != pEntry->portNum)
      return false;
    if (myEntry->eventNum != pEntry->eventNum)
      return false;
    return true;
  }
};
class ADEvntMgrProducer;
class ADEvntMgrConsumer {
public:
  virtual int receive_events(int cltToken, int evntNum, int evntArg,
                             int evntArg2) = 0;
  virtual ~ADEvntMgrConsumer(){};
};
class ADEvntMgrProducer {
  std::vector<ADEvntMgrConsumer *> subscribers;

protected:
  void notify_subscribers(int cltToken, int evntNum, int evntArg,
                          int evntArg2) {
    std::vector<ADEvntMgrConsumer *>::iterator iter;
    for (iter = subscribers.begin(); iter != subscribers.end(); ++iter)
      (*iter)->receive_events(cltToken, evntNum, evntArg, evntArg2);
  }

public:
  virtual ~ADEvntMgrProducer(){};
  void AttachReceiver(ADEvntMgrConsumer *pConsumer) {
    subscribers.push_back(pConsumer);
  }
};
class ADEvntMgr : public ADEvntMgrProducer, public ADThreadConsumer {
  int AckToken;
  int notifyThreadID;
  int processThreadID;
  std::vector<EventEntry *> eventList;
  std::deque<EventProcEntry> notifyEvent;
  std::deque<EventProcEntry> processEvent;
  ADThread EventNotifyThread;
  ADThread EventProcessThread;
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj);
  virtual int thread_callback_function(void *pUserData,
                                       ADThreadProducer *pObj) {
    return 0;
  };
  int send_event(EventEntry *pEvent, int event_num, int event_arg = -1,
                 int event_arg2 = -1);

public:
  ADEvntMgr();
  ~ADEvntMgr();
  int register_event_subscription(EventEntry *pEvent, int *ack_token);
  int unregister_event_subscription(int srv_token);
  int notify_event(int eventNum, int eventArg, int eventArg2);
  int process_event(int event_num, int event_arg, int clt_token,
                    int event_arg2);
};
#endif
