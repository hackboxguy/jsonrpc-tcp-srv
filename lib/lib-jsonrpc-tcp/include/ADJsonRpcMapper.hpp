#ifndef __AD_JSON_RPC_MAPPER_H_
#define __AD_JSON_RPC_MAPPER_H_
#include "ADGenericChain.hpp"
#include "ADJsonRpcProxy.hpp"
#include "ADThread.hpp"
class ADJsonRpcMapProducer;
class ADJsonRpcMapConsumer {
public:
  virtual int process_json_to_binary(JsonDataCommObj *pReq) = 0;
  virtual int process_work(JsonDataCommObj *pReq) = 0;
  virtual int process_binary_to_json(JsonDataCommObj *pReq) = 0;
  virtual ~ADJsonRpcMapConsumer(){};
};
class ADJsonRpcMapProducer {
  ADJsonRpcMapConsumer *pConsumerMapper;
  ADJsonRpcMapConsumer *pConsumerWorker;

protected:
  int process_json_to_binary(JsonDataCommObj *pReq) {
    if (pConsumerMapper != NULL)
      return pConsumerMapper->process_json_to_binary(pReq);
    return -1;
  }
  int process_work(JsonDataCommObj *pReq) {
    if (pConsumerWorker != NULL)
      return pConsumerWorker->process_work(pReq);
    return -1;
  }
  int process_binary_to_json(JsonDataCommObj *pReq) {
    if (pConsumerMapper != NULL)
      return pConsumerMapper->process_binary_to_json(pReq);
    return -1;
  }

public:
  ADJsonRpcMapProducer() {
    pConsumerMapper = NULL;
    pConsumerWorker = NULL;
  }
  virtual ~ADJsonRpcMapProducer(){};
  int AttachMapper(ADJsonRpcMapConsumer *c) {
    if (pConsumerMapper == NULL) {
      pConsumerMapper = c;
      return 0;
    } else
      return -1;
  }
  int AttachWorker(ADJsonRpcMapConsumer *c) {
    if (pConsumerWorker == NULL) {
      pConsumerWorker = c;
      return 0;
    } else
      return -1;
  }
};
class ADJsonRpcMapper : public ADJsonRpcConsumer,
                        public ADThreadConsumer,
                        public ADChainConsumer,
                        public ADJsonRpcMapProducer {
  ADJsonRpcProxy *pJsonProxy;
  ADThread ReqThread;
  ADGenericChain ReqChain;
  virtual int rpc_call_notification(api_task_obj *pReqRespObj);
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj);
  virtual int thread_callback_function(void *pUserData,
                                       ADThreadProducer *pObj) {
    return 0;
  };
  virtual int identify_chain_element(void *element, int ident,
                                     ADChainProducer *pObj) {
    return -1;
  };
  virtual int double_identify_chain_element(void *element, int ident1,
                                            int ident2, ADChainProducer *pObj) {
    return -1;
  };
  virtual int free_chain_element_data(void *element, ADChainProducer *pObj) {
    return 0;
  };

public:
  ADJsonRpcMapper();
  ~ADJsonRpcMapper();
  int AttachProxy(ADJsonRpcProxy *pJProxy);
  int attach_rpc_method(int index, char *method_name);
};
#endif
