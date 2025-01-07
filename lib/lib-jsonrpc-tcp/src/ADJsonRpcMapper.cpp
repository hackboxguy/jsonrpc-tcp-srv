#include "ADJsonRpcMapper.hpp"
using namespace std;
int ADJsonRpcMapper::rpc_call_notification(api_task_obj *pReqRespObj) {
  if (ReqChain.chain_put((void *)pReqRespObj) != 0) {
    printf("failed! unable to push responseID to chain!\n");
    return -1;
  }
  ReqThread.wakeup_thread();
  return 0;
}
int ADJsonRpcMapper::monoshot_callback_function(void *pUserData,
                                                ADThreadProducer *pObj) {
  api_task_obj *pApiObj = NULL;
  pApiObj = (api_task_obj *)ReqChain.chain_get();
  if (pApiObj == NULL)
    return -1;
  ADJsonRpcProxy *pJson;
  pJson = (ADJsonRpcProxy *)pApiObj->pRpcMethod->pProducer;
  if (pJson == NULL) {
    cout << (char *)"pApiObj->pRpcMethod->pProducer is NULL" << endl;
    return -1;
  }
  JsonDataCommObj DataObj;
  DataObj.req_id = pApiObj->req_id;
  DataObj.cmd_index = pApiObj->pRpcMethod->method_index;
  DataObj.mapper_result = CMD_TASK_RESULT_UNKNOWN;
  DataObj.rpc_code = RPC_SRV_RESULT_NOT_STARTED;
  DataObj.worker_result = -1;
  DataObj.socket_data = pApiObj->pNetData->data_buffer;
  DataObj.json_resp_obj = pApiObj->json_resp_obj;
  DataObj.custom_result_string = pApiObj->custom_result_string;
  DataObj.dataObjLen = 0;
  DataObj.pDataObj = NULL;
  DataObj.cltport = pApiObj->pNetData->port;
  strcpy(DataObj.ip, pApiObj->pNetData->ip);
  DataObj.sock_id = pApiObj->pNetData->cltid;
  DataObj.sock_descr = pApiObj->pNetData->sock_descriptor;
  if (process_json_to_binary(&DataObj) != 0) {
    pApiObj->task_result = DataObj.mapper_result;
    pApiObj->rpc_code = DataObj.rpc_code;
    pApiObj->json_resp_obj = DataObj.json_resp_obj;
    pJson->json_send_response(pApiObj);
    return 0;
  }
  process_work(&DataObj);
  process_binary_to_json(&DataObj);
  pApiObj->task_result = DataObj.mapper_result;
  pApiObj->rpc_code = DataObj.rpc_code;
  pApiObj->json_resp_obj = DataObj.json_resp_obj;
  pJson->json_send_response(pApiObj);
  return 0;
}
ADJsonRpcMapper::ADJsonRpcMapper() {
  pJsonProxy = NULL;
  ReqChain.disable_auto_remove();
  ReqChain.attach_helper(this);
  ReqThread.subscribe_thread_callback(this);
  ReqThread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  ReqThread.start_thread();
}
ADJsonRpcMapper::~ADJsonRpcMapper() {
  ReqThread.stop_thread();
  ReqChain.remove_all();
}
int ADJsonRpcMapper::AttachProxy(ADJsonRpcProxy *pJProxy) {
  pJsonProxy = pJProxy;
  return 0;
}
int ADJsonRpcMapper::attach_rpc_method(int index, char *method_name) {
  if (pJsonProxy == NULL) {
    cout << "ADJsonRpcMapper::attach_rpc_method: Error!! parent is not attached"
         << endl;
    return -1;
  }
  pJsonProxy->attach_rpc_method(this, method_name, index);
  return 0;
}
