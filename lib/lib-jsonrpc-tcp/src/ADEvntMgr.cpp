#include "ADEvntMgr.hpp"
#include "ADJsonRpcClient.hpp"
#include <algorithm>
ADEvntMgr::ADEvntMgr() : AckToken(0) {
  notifyThreadID = EventNotifyThread.subscribe_thread_callback(this);
  EventNotifyThread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  EventNotifyThread.start_thread();
  processThreadID = EventProcessThread.subscribe_thread_callback(this);
  EventProcessThread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  EventProcessThread.start_thread();
}
ADEvntMgr::~ADEvntMgr() {
  EventProcessThread.stop_thread();
  EventNotifyThread.stop_thread();
  for (int i = 0; i < eventList.size(); ++i)
    delete eventList[i];
}
int ADEvntMgr::monoshot_callback_function(void *pUserData,
                                          ADThreadProducer *pObj) {
  int call_from = pObj->getID();
  if (call_from == notifyThreadID) {
    // lock held while sending: eventList must not change underneath, and the
    // sends go to local subscribers with a bounded connect timeout
    std::lock_guard<std::mutex> lock(evntMutex);
    while (!notifyEvent.empty()) {
      EventProcEntry entry = notifyEvent.front();
      for (int i = 0; i < eventList.size(); ++i) {
        if (eventList[i]->eventNum == -1) {
          if (send_event(eventList[i], entry.eventNum, entry.eventArg,
                         entry.eventArg2) == -1)
            eventList[i]->deleteFlag = true;
        } else if (eventList[i]->eventNum == entry.eventNum) {
          if (send_event(eventList[i], entry.eventNum, entry.eventArg,
                         entry.eventArg2) == -1)
            eventList[i]->deleteFlag = true;
        }
      }
      RemoveNonListenerEntry Remover;
      eventList.erase(remove_if(eventList.begin(), eventList.end(), Remover),
                      eventList.end());
      notifyEvent.pop_front();
    }
  } else {
    while (true) {
      EventProcEntry entry(0, 0, 0, 0);
      {
        std::lock_guard<std::mutex> lock(evntMutex);
        if (processEvent.empty())
          break;
        entry = processEvent.front();
        processEvent.pop_front();
      }
      // subscribers run application code: call them without the lock
      notify_subscribers(entry.cltToken, entry.eventNum, entry.eventArg,
                         entry.eventArg2);
    }
  }
  return 0;
}
int ADEvntMgr::register_event_subscription(EventEntry *pEvent, int *ack_token) {
  std::lock_guard<std::mutex> lock(evntMutex);
  if (find_if(eventList.begin(), eventList.end(), FindDuplicateEntry(pEvent)) ==
      eventList.end()) {
    pEvent->srvToken = *ack_token = ++AckToken;
    eventList.push_back(pEvent);
    PrintEventEntries PrintEvent;
    for_each(eventList.begin(), eventList.end(), PrintEvent);
    cout << "##############################################" << endl;
    return 0;
  } else
    return -1;
}
int ADEvntMgr::unregister_event_subscription(int srv_token) {
  std::lock_guard<std::mutex> lock(evntMutex);
  if (find_if(eventList.begin(), eventList.end(), FindEventEntry(srv_token)) ==
      eventList.end())
    return -1;
  eventList.erase(remove_if(eventList.begin(), eventList.end(),
                            RemoveEventEntry(srv_token)),
                  eventList.end());
  cout << "unsubscribe of srvToken = " << srv_token << " success" << endl;
  return 0;
}
int ADEvntMgr::notify_event(int eventNum, int eventArg, int eventArg2) {
  {
    std::lock_guard<std::mutex> lock(evntMutex);
    notifyEvent.push_back(EventProcEntry(eventNum, eventArg, 0, eventArg2));
  }
  EventNotifyThread.wakeup_thread();
  return 0;
}
int ADEvntMgr::send_event(EventEntry *pEvent, int event_num, int event_arg,
                          int event_arg2) {
  ADJsonRpcClient Client;
  if (Client.rpc_server_connect(pEvent->ip, pEvent->portNum) != 0)
    return -1;
  Client.set_four_int_type(
      (char *)RPCMGR_RPC_EVENT_PROCESS, (char *)RPCMGR_RPC_EVENT_ARG_CLTTOK,
      pEvent->cltToken, (char *)RPCMGR_RPC_EVENT_ARG_EVENTNUM, event_num,
      (char *)RPCMGR_RPC_EVENT_ARG_EXTRA, event_arg,
      (char *)RPCMGR_RPC_EVENT_ARG2_EXTRA, event_arg2);
  Client.rpc_server_disconnect();
  return 0;
}
int ADEvntMgr::process_event(int event_num, int event_arg, int clt_token,
                             int event_arg2) {
  {
    std::lock_guard<std::mutex> lock(evntMutex);
    processEvent.push_back(
        EventProcEntry(event_num, event_arg, clt_token, event_arg2));
  }
  EventProcessThread.wakeup_thread();
  return 0;
}
