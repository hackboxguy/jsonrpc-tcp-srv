#ifndef __EVENT_HANDLER_H_
#define __EVENT_HANDLER_H_
#include "ADCmnStringProcessor.hpp"
#include "ADJsonRpcMgr.hpp"
#include "SysmgrJsonDef.h"
/* ------------------------------------------------------------------------- */
class EventHandler : public ADJsonRpcMgrConsumer, public ADCmnStringProcessor {
  int srvToken; //=-1;
  SYSMGR_CMN_DATA_CACHE *pDataCache;
  bool gpioEventActive;

public:
  EventHandler(std::string rpcName, int myIndex, bool emu, bool log,
               SYSMGR_CMN_DATA_CACHE *pData);
  ~EventHandler();
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
