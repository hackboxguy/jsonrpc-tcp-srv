#ifndef __ADJSONRPCPROXY_H_
#define __ADJSONRPCPROXY_H_
#include "ADGenericChain.hpp"
#include "ADJsonChecker.hpp"
#include "ADNetServer.hpp"
#include "JsonCmnDef.h"
#include <iostream>
#include <string>
#include <typeinfo>
#include <vector>
using namespace std;
typedef enum cmd_task_access_t {
  CMD_TASK_ACCESS_SYNC,
  CMD_TASK_ACCESS_ASYNC,
  CMD_TASK_ACCESS_NONE
} CMD_TASK_ACC;
#define JSON_RPC_VERSION "2.0"
#define JSON_RPC_PARSE_ERR_MSG "Parse error."
#define JSON_RPC_INVALID_REQ_MSG "Invalid Request."
#define JSON_RPC_METHOD_NOT_FOUND_MSG "Method not found."
#define JSON_RPC_INVALID_PARAMS_MSG "Invalid params."
#define JSON_RPC_INTERNAL_ERR_MSG "Internal error."
typedef enum JSON_RPC_ERR_TYPE_T {
  JSON_RPC_ERR_PARSE_ERROR = -32700,
  JSON_RPC_ERR_INVALID_REQ = -32600,
  JSON_RPC_ERR_METHOD_NOT_FOUND = -32601,
  JSON_RPC_ERR_INVALID_PARAMS = -32602,
  JSON_RPC_ERR_INTERNAL_ERROR = -32603,
  JSON_RPC_ERR_UNKNOWN_ERROR = -33000
} JSON_RPC_ERR_TYPE;
#define JSON_RESULT_SUCCESS "OK"
#define JSON_RESULT_FAIL "FAIL"
#define JSON_RESULT_UNKNOWN "UNKNOWN"
#define TASK_RESULT_STRING_NAME "result"
#define TASK_RESULT_STRING_FAIL "Fail"
#define TASK_RESULT_STRING_SUCCESS "Success"
struct api_task_obj;
class ADJsonRpcProducer;
class ADJsonRpcConsumer {
public:
  virtual int rpc_call_notification(api_task_obj *pReqRespObj) = 0;
  virtual ~ADJsonRpcConsumer(){};
};
struct ADJsonRpcCommand {
  ADJsonRpcConsumer *pParent;
  char method[JSON_RPC_METHOD_NAME_MAX_LENGTH];
  int method_index;
  ADJsonRpcProducer *pProducer;

public:
  ADJsonRpcCommand(){};
  ~ADJsonRpcCommand(){};
};
class ADJsonRpcProducer {
  std::vector<ADJsonRpcCommand *> rpc_call_list;

protected:
  ADJsonRpcCommand *is_this_method_registered(char *rpc_method) {
    std::vector<ADJsonRpcCommand *>::iterator iter;
    for (iter = rpc_call_list.begin(); iter != rpc_call_list.end(); ++iter) {
      if (strcmp((*iter)->method, rpc_method) == 0)
        return (*iter);
    }
    return NULL;
  }

public:
  virtual ~ADJsonRpcProducer() {
    std::vector<ADJsonRpcCommand *>::iterator iter;
    for (iter = rpc_call_list.begin(); iter != rpc_call_list.end(); ++iter)
      OBJ_MEM_DELETE((*iter));
  };
  int attach_rpc_method(ADJsonRpcConsumer *pConsumer, char *method_name,
                        int method_indx) {
    ADJsonRpcCommand *pList = NULL;
    if (strlen(method_name) >= JSON_RPC_METHOD_NAME_MAX_LENGTH)
      return -1;
    OBJECT_MEM_NEW(pList, ADJsonRpcCommand);
    if (pList != NULL) {
      pList->pParent = pConsumer;
      strcpy(pList->method, method_name);
      pList->method_index = method_indx;
      pList->pProducer = this;
      rpc_call_list.push_back(pList);
      return 0;
    } else
      return -1;
  }
};
struct api_task_obj {
  int ident;
  int req_id;
  net_data_obj *pNetData;
  ADJsonRpcCommand *pRpcMethod;
  CMD_TASK_ACC access;
  CMD_TASK_RESULT task_result;
  RPC_SRV_RESULT rpc_code;
  RPC_SRV_ACT rpc_action;
  int result_code;
  json_object *json_resp_obj;
  char *json_resp_string;
  int json_resp_string_len;
  char custom_result_string[JSON_RPC_METHOD_RESP_MAX_LENGTH];

public:
  api_task_obj(){};
  ~api_task_obj(){};
};
class ADJsonRpcProxy : public ADJsonRpcProducer,
                       public ADNetConsumer,
                       public ADChainConsumer,
                       public ADThreadConsumer {
  char request_timestamp[255];
  int socketlog;
  int total_req_received;
  int total_res_sent;
  ADNetServer ServerSocket;
  ADGenericChain ReqRespChain;
  int ReqRespChainID;
  ADGenericChain RespChain;
  int RespChainID;
  ADThread RespThread;
  virtual int on_data_arrival(ADGenericChain *pRxChain, ADNetProducer *pObj);
  virtual int identify_chain_element(void *element, int ident,
                                     ADChainProducer *pObj);
  virtual int double_identify_chain_element(void *element, int ident1,
                                            int ident2, ADChainProducer *pObj);
  virtual int free_chain_element_data(void *element, ADChainProducer *pObj);
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj);
  virtual int thread_callback_function(void *pUserData, ADThreadProducer *pObj);
  char *get_timestamp();
  int json_checker_function(char *string);
  void print_json_value(json_object *jobj, json_type type);
  void json_parse(json_object *obj);
  void json_parse_array(json_object *jobj, char *key);
  int check_jsonrpc_2_0(struct json_object *new_obj);
  ADJsonRpcCommand *get_jsonrpc_method(struct json_object *new_obj);
  CMD_TASK_ACC get_jsonrpc_acc(struct json_object *new_obj);
  int get_jsonrpc_id(struct json_object *new_obj, int *req_id);
  int json_send_error_response_string(int id, int sock_descriptor,
                                      JSON_RPC_ERR_TYPE err_type);
  int json_send_result_response_string(int id, int sock_descriptor,
                                       char *result, api_task_obj *task_obj);
  int json_single_string_response_sender(api_task_obj *resp_obj);
  int json_send_back_result(api_task_obj *resp_obj);
  int json_process_request(net_data_obj *req_obj);
  int json_process_rsponse();
  int json_send_new_result(api_task_obj *resp_obj);

public:
  ADJsonRpcProxy();
  ~ADJsonRpcProxy();
  int start_listening(int port, int socket_log);
  int json_send_response(struct api_task_obj *pTaskObj);
};
#endif
