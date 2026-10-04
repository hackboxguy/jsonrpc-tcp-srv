#ifndef __ADEVNT_MGR_H_
#define __ADEVNT_MGR_H_
#include "ADThread.hpp"
#include <deque>
#include <iostream>
#include <pthread.h>
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
  int failCount;    // consecutive failed deliveries (set by ADEvntMgr)
  long firstFailMs; // monotonic time of the first of those failures
} EventEntry;
// Event delivery limits (findings V2-H4, V3-M2): a subscriber is removed
// only when its deliveries have failed continuously for
// ADEVNT_MGR_FAIL_WINDOW_MS and at least ADEVNT_MGR_MAX_FAILURES times, so a
// subscriber that is busy or restarting for a while keeps its subscription.
// Failed events are not retried. Subscribers should re-subscribe after
// their own restart (a duplicate subscription is answered with
// ITEM_DUPLICATE_FOUND and is harmless). Each delivery is bounded by the two
// timeouts; at most ADEVNT_MGR_MAX_QUEUED events wait (oldest dropped).
#define ADEVNT_MGR_MAX_FAILURES 5
#define ADEVNT_MGR_FAIL_WINDOW_MS 60000
#define ADEVNT_MGR_CONNECT_TIMEOUT_MS 1000
#define ADEVNT_MGR_RECEIVE_TIMEOUT_MS 1000
#define ADEVNT_MGR_MAX_QUEUED 1024
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
// eventList, notifyEvent and processEvent are written by the RPC thread and
// read by the notify/process threads; all access holds 'lock'. Network I/O
// (send_event) and consumer callbacks run without the lock (finding C5).
class ADEvntMgr : public ADEvntMgrProducer, public ADThreadConsumer {
  int AckToken;
  int notifyThreadID;
  int processThreadID;
  pthread_mutex_t lock;
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
  void update_failure_counts(const std::vector<int> &failed,
                             const std::vector<int> &delivered);

public:
  ADEvntMgr();
  ~ADEvntMgr();
  void stop(); // stops both threads; idempotent
  int register_event_subscription(EventEntry *pEvent, int *ack_token);
  int unregister_event_subscription(int srv_token);
  int notify_event(int eventNum, int eventArg, int eventArg2);
  int process_event(int event_num, int event_arg, int clt_token,
                    int event_arg2);
};
#endif
