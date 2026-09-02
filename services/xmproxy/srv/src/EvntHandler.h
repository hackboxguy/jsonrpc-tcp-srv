#ifndef __EVNT_HANDLER_H_
#define __EVNT_HANDLER_H_
#include "ADCmnStringProcessor.hpp"
#include "ADJsonRpcMgr.hpp"
#include "ADTimer.hpp"
#include "XmproxyJsonDef.h"
#include <mutex>
/* ------------------------------------------------------------------------- */
// Receives async completion and gpio events from peer services. Subscribes
// at start and, on the 100 ms heartbeat, re-subscribes every
// EVNT_RESUBSCRIBE_PERIOD_MS: a peer that restarted (or crashed) has lost
// our subscription and would otherwise never deliver completions again (F2).
#define EVNT_RESUBSCRIBE_PERIOD_MS 30000
class EvntHandler : public ADJsonRpcMgrConsumer,
                    public ADCmnStringProcessor,
                    public ADTimerConsumer {
  struct Peer {
    const char *name;
    int port;
    bool active;
    int srvToken;
  };
  Peer peers[3];
  std::mutex peerMutex;
  int resubscribe_ms;
  bool debugLog;
  void subscribe_peer(Peer &peer, bool quiet);
  void resubscribe_all();

  XMPROXY_CMN_DATA_CACHE *pDataCache;

public:
  EvntHandler(std::string rpcName, int myIndex, bool emu, bool log,
              XMPROXY_CMN_DATA_CACHE *pData);
  ~EvntHandler();
  int AttachHeartBeat(ADTimer *pTimer);
  virtual int sigio_notification() { return 0; };
  virtual int timer_notification();
  virtual int custom_sig_notification(int signum) { return 0; };
  virtual int MapJsonToBinary(JsonDataCommObj *pReq, int index) { return -1; };
  virtual int MapBinaryToJson(JsonDataCommObj *pReq, int index) { return -1; };
  virtual int ProcessWork(JsonDataCommObj *pReq, int index,
                          ADJsonRpcMgrProducer *pObj) {
    return -1;
  };
  virtual RPC_SRV_RESULT ProcessWorkAsync(int index, unsigned char *pWorkData) {
    return RPC_SRV_RESULT_FAIL;
  };
  virtual void ReceiveEvent(int cltToken, int evntNum, int evntArg,
                            int evntArg2);
  // virtual RPC_SRV_RESULT ProcessCommonRpc(int index,unsigned char*
  // pWorkData){return RPC_SRV_RESULT_SUCCESS;};
};
/* ------------------------------------------------------------------------- */

#endif
