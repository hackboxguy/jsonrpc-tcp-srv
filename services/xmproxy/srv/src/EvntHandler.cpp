#include "EvntHandler.h"
#include "ADCmnPortList.h"
#include "ADJsonRpcClient.hpp"
#include "XmLog.h"
#include "XmppMgr.h"
#define EVENT_SYSMGR ADCMN_PORT_SYSMGR // 40001
#define EVENT_BBXSMS ADCMN_PORT_BBOXSMS
#define EVENT_GPIOCTL ADCMN_PORT_GPIOCTL
/* ------------------------------------------------------------------------- */
EvntHandler::EvntHandler(std::string rpcName, int myIndex, bool emu, bool log,
                         XMPROXY_CMN_DATA_CACHE *pData)
    : ADJsonRpcMgrConsumer(rpcName, myIndex, emu, log) {
  pDataCache = pData;
  debugLog = log;
  resubscribe_ms = 0;
  // second arg: port number of the peer; the peer returns a unique token on
  // success (srvToken); cltToken tells the peer who we are so that events
  // carry it back; eventNum -1 means all events; last arg is our own port
  // where the peer delivers events.
  Peer init[3] = {{"sysmgr", EVENT_SYSMGR, false, -1},
                  {"bboxsms", EVENT_BBXSMS, false, -1},
                  {"gpio", EVENT_GPIOCTL, false, -1}};
  for (int i = 0; i < 3; i++)
    peers[i] = init[i];
  // the first subscribe happens in AttachHeartBeat(), once our own RPC
  // server is listening and can receive the events
}
/* ------------------------------------------------------------------------- */
EvntHandler::~EvntHandler() {
  std::lock_guard<std::mutex> lock(peerMutex);
  for (int i = 0; i < 3; i++) {
    if (peers[i].active) // unsubscribe only if subscription is active
      UNSUBSCRIBE_EVENT("127.0.0.1", peers[i].port, peers[i].srvToken);
  }
}
/* ------------------------------------------------------------------------- */
// One subscribe attempt. The peer rejects a duplicate subscription (same
// client token, port and event), so calling this while already subscribed is
// harmless; after a peer restart it succeeds and yields a fresh token.
void EvntHandler::subscribe_peer(Peer &peer, bool quiet) {
#ifndef USE_LEGACY_GSM
  if (peer.port == EVENT_BBXSMS)
    return; // the SMS service belongs to the legacy GSM feature set
#endif
  int token = -1;
  {
    // same RPC as the library's SUBSCRIBE_EVENT macro, but a refused
    // duplicate (already subscribed) or an absent peer is normal here and
    // must not print a line every period
    ADJsonRpcClient Client;
    if (Client.rpc_server_connect("127.0.0.1", peer.port) == 0) {
      if (Client.set_three_int_get_one_int(
              (char *)RPCMGR_RPC_EVENT_SUBSCRIBE,
              (char *)RPCMGR_RPC_EVENT_ARG_CLTTOK, peer.port,
              (char *)RPCMGR_RPC_EVENT_ARG_PORT, XMPROXY_JSON_PORT_NUMBER,
              (char *)RPCMGR_RPC_EVENT_ARG_EVENTNUM, -1,
              (char *)RPCMGR_RPC_EVENT_ARG_SRVTOK,
              &token) != RPC_SRV_RESULT_SUCCESS)
        token = -1; // refused: already subscribed, or peer without events
      Client.rpc_server_disconnect();
    }
  }
  if (token != -1) {
    if (peer.active && peer.srvToken != token)
      XMLOG_WRN("events: %s restarted, re-subscribed (token %d -> %d)",
                peer.name, peer.srvToken, token);
    else if (!peer.active)
      XMLOG_INF("events: subscribed to %s (token %d)", peer.name, token);
    peer.active = true;
    peer.srvToken = token;
  } else if (!quiet) {
    XMLOG_INF("events: %s not available on port %d", peer.name, peer.port);
  }
}
void EvntHandler::resubscribe_all() {
  std::lock_guard<std::mutex> lock(peerMutex);
  for (int i = 0; i < 3; i++)
    subscribe_peer(peers[i], true);
}
/* ------------------------------------------------------------------------- */
int EvntHandler::AttachHeartBeat(ADTimer *pTimer) {
  {
    std::lock_guard<std::mutex> lock(peerMutex);
    for (int i = 0; i < 3; i++)
      subscribe_peer(peers[i], false);
  }
  pTimer->subscribe_timer_notification(this);
  return 0;
}
int EvntHandler::timer_notification() {
  resubscribe_ms += 100;
  if (resubscribe_ms < EVNT_RESUBSCRIBE_PERIOD_MS)
    return 0;
  resubscribe_ms = 0;
  resubscribe_all();
  return 0;
}
/* ------------------------------------------------------------------------- */
void EvntHandler::ReceiveEvent(int cltToken, int evntNum, int evntArg,
                               int evntArg2) {
  if (evntNum == ADLIB_EVENT_NUM_SHUT_DOWN) {
    std::lock_guard<std::mutex> lock(peerMutex);
    for (int i = 0; i < 3; i++) {
      if (peers[i].port == cltToken && peers[i].active) {
        XMLOG_WRN("events: %s shut down, subscription lost", peers[i].name);
        peers[i].active = false; // re-subscribed by the heartbeat
      }
    }
  }

  if (evntNum == ADLIB_EVENT_NUM_INPROG_DONE) {
    char taskIDString[255];
    char taskIDResult[255];
    taskIDResult[254] = '\0';
    snprintf(taskIDString, sizeof(taskIDString), "%d", evntArg);
    XmppMgr *pXmpp = (XmppMgr *)pDataCache->pXmpMgr;
    int xmpTID = -1;
    std::string to;
    bool json = false;
    std::string reqId, pollControl, pollInitialTo;
    bool shellOutput = false;
    if (pXmpp->AccessAsyncTaskList(evntArg, cltToken, false, &xmpTID, to, &json,
                                   &reqId, &pollControl, &pollInitialTo,
                                   &shellOutput) == RPC_SRV_RESULT_SUCCESS) {
      ADJsonRpcClient Client;
      if (Client.rpc_server_connect("127.0.0.1", cltToken) != 0) {
        XMLOG_ERR("events: cannot connect to peer on port %d for task %d",
                  cltToken, evntArg);
        pXmpp->RpcResponseCallback(RPC_SRV_RESULT_HOST_NOT_REACHABLE_ERR,
                                   xmpTID, to, json, reqId, pollControl,
                                   pollInitialTo, shellOutput);
        return;
      }
      Client.get_string_type_with_string_para(
          (char *)ADLIB_RPC_NAME_GET_TASK_STATUS,
          (char *)ADLIB_RPC_PARM_TASK_STS_ID, taskIDString, taskIDResult,
          (char *)ADLIB_RPC_PARM_TASK_STS);
      Client.rpc_server_disconnect();
      std::string finalRes = taskIDResult;
      pXmpp->RpcResponseCallback(finalRes, xmpTID, to, json, reqId, pollControl,
                                 pollInitialTo, shellOutput);
    } else {
      XMLOG_DBG("events: completion for unknown task %d from port %d", evntArg,
                cltToken);
    }
  } else if (cltToken == EVENT_GPIOCTL &&
             evntNum == ADLIB_EVENT_NUM_END) // TODO:correctly compare evntNum
                                             // with actual enum of gpio-srv
  {
    XmppMgr *pXmpp = (XmppMgr *)pDataCache->pXmpMgr;
    pXmpp->GpioEventCallback(evntNum, evntArg);
  }
}
/* ------------------------------------------------------------------------- */
