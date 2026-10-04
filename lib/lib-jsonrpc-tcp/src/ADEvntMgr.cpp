#include "ADEvntMgr.hpp"
#include "ADCommon.hpp"
#include "ADJsonRpcClient.hpp"
#include <algorithm>
ADEvntMgr::ADEvntMgr() : AckToken(0) {
  pthread_mutex_init(&lock, NULL);
  notifyThreadID = EventNotifyThread.subscribe_thread_callback(this);
  EventNotifyThread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  EventNotifyThread.start_thread();
  processThreadID = EventProcessThread.subscribe_thread_callback(this);
  EventProcessThread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  EventProcessThread.start_thread();
}
ADEvntMgr::~ADEvntMgr() {
  stop();
  for (size_t i = 0; i < eventList.size(); ++i)
    delete eventList[i];
  pthread_mutex_destroy(&lock);
}
void ADEvntMgr::stop() {
  EventProcessThread.stop_thread();
  EventNotifyThread.stop_thread();
}
int ADEvntMgr::monoshot_callback_function(void *pUserData,
                                          ADThreadProducer *pObj) {
  int call_from = pObj->getID();
  if (call_from == notifyThreadID) {
    std::deque<EventProcEntry> pending;
    pthread_mutex_lock(&lock);
    pending.swap(notifyEvent);
    pthread_mutex_unlock(&lock);
    while (!pending.empty()) {
      EventProcEntry entry = pending.front();
      pending.pop_front();
      // copy the matching subscribers so no lock is held during network I/O
      std::vector<EventEntry> targets;
      pthread_mutex_lock(&lock);
      for (size_t i = 0; i < eventList.size(); ++i) {
        if (eventList[i]->eventNum == -1 ||
            eventList[i]->eventNum == entry.eventNum)
          targets.push_back(*eventList[i]);
      }
      pthread_mutex_unlock(&lock);
      std::vector<int> failed, delivered;
      for (size_t i = 0; i < targets.size(); ++i) {
        if (EventNotifyThread.stop_requested())
          return 0; // shutting down: do not wait for more subscribers
        if (send_event(&targets[i], entry.eventNum, entry.eventArg,
                       entry.eventArg2) == -1)
          failed.push_back(targets[i].srvToken);
        else
          delivered.push_back(targets[i].srvToken);
      }
      update_failure_counts(failed, delivered);
    }
  } else {
    std::deque<EventProcEntry> pending;
    pthread_mutex_lock(&lock);
    pending.swap(processEvent);
    pthread_mutex_unlock(&lock);
    while (!pending.empty()) {
      EventProcEntry entry = pending.front();
      pending.pop_front();
      notify_subscribers(entry.cltToken, entry.eventNum, entry.eventArg,
                         entry.eventArg2);
    }
  }
  return 0;
}
// A subscriber that is restarting must not lose its subscription because of
// one failed delivery: it is removed only after ADEVNT_MGR_MAX_FAILURES
// consecutive failures.
void ADEvntMgr::update_failure_counts(const std::vector<int> &failed,
                                      const std::vector<int> &delivered) {
  if (failed.empty() && delivered.empty())
    return;
  pthread_mutex_lock(&lock);
  for (size_t i = 0; i < eventList.size(); ++i) {
    EventEntry *e = eventList[i];
    if (find(delivered.begin(), delivered.end(), e->srvToken) !=
        delivered.end())
      e->failCount = 0;
    else if (find(failed.begin(), failed.end(), e->srvToken) != failed.end()) {
      if (++e->failCount >= ADEVNT_MGR_MAX_FAILURES) {
        LOG_ERR_MSG_WITH_ARG("libadav:ADEvntMgr",
                             "removing event subscriber on port %d after "
                             "repeated delivery failures",
                             e->portNum);
        e->deleteFlag = true;
      }
    }
  }
  eventList.erase(
      remove_if(eventList.begin(), eventList.end(), RemoveNonListenerEntry()),
      eventList.end());
  pthread_mutex_unlock(&lock);
}
int ADEvntMgr::register_event_subscription(EventEntry *pEvent, int *ack_token) {
  pEvent->failCount = 0;
  pEvent->deleteFlag = false;
  pthread_mutex_lock(&lock);
  if (find_if(eventList.begin(), eventList.end(), FindDuplicateEntry(pEvent)) ==
      eventList.end()) {
    pEvent->srvToken = *ack_token = ++AckToken;
    eventList.push_back(pEvent);
    PrintEventEntries PrintEvent;
    for_each(eventList.begin(), eventList.end(), PrintEvent);
    pthread_mutex_unlock(&lock);
    cout << "##############################################" << endl;
    return 0;
  }
  pthread_mutex_unlock(&lock);
  return -1;
}
int ADEvntMgr::unregister_event_subscription(int srv_token) {
  pthread_mutex_lock(&lock);
  if (find_if(eventList.begin(), eventList.end(), FindEventEntry(srv_token)) ==
      eventList.end()) {
    pthread_mutex_unlock(&lock);
    return -1;
  }
  eventList.erase(remove_if(eventList.begin(), eventList.end(),
                            RemoveEventEntry(srv_token)),
                  eventList.end());
  pthread_mutex_unlock(&lock);
  cout << "unsubscribe of srvToken = " << srv_token << " success" << endl;
  return 0;
}
int ADEvntMgr::notify_event(int eventNum, int eventArg, int eventArg2) {
  bool dropped = false;
  pthread_mutex_lock(&lock);
  if (notifyEvent.size() >= ADEVNT_MGR_MAX_QUEUED) {
    notifyEvent.pop_front();
    dropped = true;
  }
  notifyEvent.push_back(EventProcEntry(eventNum, eventArg, 0, eventArg2));
  pthread_mutex_unlock(&lock);
  if (dropped)
    LOG_ERR_MSG("libadav:ADEvntMgr", "event queue full, oldest event dropped");
  EventNotifyThread.wakeup_thread();
  return 0;
}
int ADEvntMgr::send_event(EventEntry *pEvent, int event_num, int event_arg,
                          int event_arg2) {
  ADJsonRpcClient Client;
  // short bounds: one dead subscriber must not delay all others (V2-H4)
  Client.set_connect_timeout(ADEVNT_MGR_CONNECT_TIMEOUT_MS);
  Client.set_receive_timeout(ADEVNT_MGR_RECEIVE_TIMEOUT_MS);
  if (Client.rpc_server_connect(pEvent->ip, pEvent->portNum) != 0)
    return -1;
  RPC_SRV_RESULT res = Client.set_four_int_type(
      (char *)RPCMGR_RPC_EVENT_PROCESS, (char *)RPCMGR_RPC_EVENT_ARG_CLTTOK,
      pEvent->cltToken, (char *)RPCMGR_RPC_EVENT_ARG_EVENTNUM, event_num,
      (char *)RPCMGR_RPC_EVENT_ARG_EXTRA, event_arg,
      (char *)RPCMGR_RPC_EVENT_ARG2_EXTRA, event_arg2);
  Client.rpc_server_disconnect();
  return res == RPC_SRV_RESULT_SUCCESS ? 0 : -1;
}
int ADEvntMgr::process_event(int event_num, int event_arg, int clt_token,
                             int event_arg2) {
  bool dropped = false;
  pthread_mutex_lock(&lock);
  if (processEvent.size() >= ADEVNT_MGR_MAX_QUEUED) {
    processEvent.pop_front();
    dropped = true;
  }
  processEvent.push_back(
      EventProcEntry(event_num, event_arg, clt_token, event_arg2));
  pthread_mutex_unlock(&lock);
  if (dropped)
    LOG_ERR_MSG("libadav:ADEvntMgr", "event queue full, oldest event dropped");
  EventProcessThread.wakeup_thread();
  return 0;
}
