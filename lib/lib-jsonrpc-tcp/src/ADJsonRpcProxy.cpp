#include "ADJsonRpcProxy.hpp"
#include "ADCommon.hpp"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace std;
int ADJsonRpcProxy::on_data_arrival(ADGenericChain *pRxChain,
                                    ADNetProducer *pObj) {
  net_data_obj *req_obj;
  while ((req_obj = (net_data_obj *)pRxChain->chain_get()) != NULL) {
    if (json_process_request(req_obj) != 0) {
      ARRAY_MEM_DELETE(req_obj->data_buffer);
      OBJ_MEM_DELETE(req_obj);
    }
  }
  return 0;
}
int ADJsonRpcProxy::identify_chain_element(void *element, int ident,
                                           ADChainProducer *pObj) {
  if (pObj->getID() == ReqRespChainID) {
    struct api_task_obj *pTaskObj;
    pTaskObj = (api_task_obj *)element;
    if (pTaskObj->ident == ident)
      return 0;
  }
  return -1;
}
int ADJsonRpcProxy::double_identify_chain_element(void *element, int ident1,
                                                  int ident2,
                                                  ADChainProducer *pObj) {
  return -1;
}
int ADJsonRpcProxy::free_chain_element_data(void *element,
                                            ADChainProducer *pObj) {
  if (pObj->getID() == ReqRespChainID) {
    api_task_obj *objData;
    objData = (api_task_obj *)element;
    if (objData->pNetData->data_buffer != NULL)
      MEM_DELETE(objData->pNetData->data_buffer);
    if (objData->pNetData != NULL)
      MEM_DELETE(objData->pNetData);
    if (objData->json_resp_obj != NULL)
      MEM_DELETE(objData->json_resp_obj);
    if (objData->json_resp_string != NULL)
      MEM_DELETE(objData->json_resp_string);
  }
  return 0;
}
int ADJsonRpcProxy::monoshot_callback_function(void *pUserData,
                                               ADThreadProducer *pObj) {
  json_process_rsponse();
  return 0;
}
int ADJsonRpcProxy::thread_callback_function(void *pUserData,
                                             ADThreadProducer *pObj) {
  return 0;
}
ADJsonRpcProxy::ADJsonRpcProxy() {
  socketlog = 0;
  total_req_received = 0;
  total_res_sent = 0;
  ServerSocket.attach_on_data_arrival(this);
  ReqRespChainID = ReqRespChain.attach_helper(this);
  RespChainID = RespChain.attach_helper(this);
  RespThread.subscribe_thread_callback(this);
  RespThread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  RespThread.start_thread();
}
ADJsonRpcProxy::~ADJsonRpcProxy() {
  cout << "ADJsonRpcProxy: total_req_received=" << total_req_received
       << " total_res_sent=" << total_res_sent << endl;
  RespThread.stop_thread();
  RespChain.remove_all();
}
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
char *ADJsonRpcProxy::get_timestamp() {
  char buffer[30];
  struct timeval tv;
  time_t curtime;
  request_timestamp[0] = '\0';
  gettimeofday(&tv, NULL);
  curtime = tv.tv_sec;
  strftime(buffer, 30, "%T.", localtime(&curtime));
  sprintf(request_timestamp, "%s%03ld", buffer, (tv.tv_usec / 1000));
  return request_timestamp;
}
int ADJsonRpcProxy::start_listening(int port, int socket_log) {
  socketlog = socket_log;
  return ServerSocket.start_listening(port, socket_log);
}
int ADJsonRpcProxy::json_checker_function(char *string) {
  int i, str_len;
  str_len = strlen(string);
  JSON_checker jc = new_JSON_checker(20);
  for (i = 0; i < str_len; i++) {
    int next_char = string[i];
    if (next_char <= 0) {
      break;
    }
    if (!JSON_checker_char(jc, next_char))
      return -1;
  }
  if (!JSON_checker_done(jc))
    return -1;
  return 0;
}
void ADJsonRpcProxy::print_json_value(json_object *jobj, json_type type) {
  printf("type: ");
  switch (type) {
  case json_type_boolean:
    printf("json_type_boolean\n");
    printf("value: %s\n", json_object_get_boolean(jobj) ? "true" : "false");
    break;
  case json_type_double:
    printf("json_type_double\n");
    printf("          value: %lf\n", json_object_get_double(jobj));
    break;
  case json_type_int:
    printf("json_type_int\n");
    printf("          value: %d\n", json_object_get_int(jobj));
    break;
  case json_type_string:
    printf("json_type_string\n");
    printf("          value: %s\n", json_object_get_string(jobj));
    break;
  case json_type_null:
  case json_type_object:
  case json_type_array:
    break;
  }
}
void ADJsonRpcProxy::json_parse(json_object *obj) {
  char *key;
  struct json_object *val;
  struct lh_entry *entry;
  enum json_type type;
  for (entry = json_object_get_object(obj)->head;
       (entry ? (key = (char *)entry->k, val = (struct json_object *)entry->v,
                entry)
              : 0);
       entry = entry->next) {
    printf("type: ");
    type = json_object_get_type(val);
    switch (type) {
    case json_type_boolean:
    case json_type_double:
    case json_type_int:
    case json_type_string:
      print_json_value(val, type);
      break;
    case json_type_object: {
      printf("json_type_object\n");
      json_object *jobj2 = json_object_object_get(obj, key);
      json_parse(jobj2);
    } break;
    case json_type_array:
      printf("type: json_type_array, ");
      json_parse_array(obj, key);
      break;
    case json_type_null:
      break;
    default:
      break;
    }
  }
}
void ADJsonRpcProxy::json_parse_array(json_object *jobj, char *key) {
  enum json_type type;
  int i;
  int arraylen;
  json_object *jarray = jobj;
  if (key) {
    jarray = json_object_object_get(jobj, key);
  }
  arraylen = json_object_array_length(jarray);
  printf("Array Length: %d\n", arraylen);
  json_object *jvalue;
  for (i = 0; i < arraylen; i++) {
    jvalue = json_object_array_get_idx(jarray, i);
    type = json_object_get_type(jvalue);
    if (type == json_type_array) {
      json_parse_array(jvalue, NULL);
    } else if (type != json_type_object) {
      printf("value[%d]: ", i);
      print_json_value(jvalue, type);
    } else {
      json_parse(jvalue);
    }
  }
}
int ADJsonRpcProxy::check_jsonrpc_2_0(struct json_object *new_obj) {
  char tmp_arr[256];
  new_obj = json_object_object_get(new_obj, "jsonrpc");
  if (json_object_get_string(new_obj) == NULL)
    return -1;
  sprintf(tmp_arr, "%s", json_object_get_string(new_obj));
  if (strcmp(tmp_arr, "2.0") == 0)
    return 0;
  else
    return -1;
}
ADJsonRpcCommand *
ADJsonRpcProxy::get_jsonrpc_method(struct json_object *new_obj) {
  char tmp_arr[JSON_RPC_METHOD_NAME_MAX_LENGTH];
  new_obj = json_object_object_get(new_obj, "method");
  if (json_object_get_string(new_obj) == NULL)
    return NULL;
  sprintf(tmp_arr, "%s", json_object_get_string(new_obj));
  return (is_this_method_registered(tmp_arr));
}
CMD_TASK_ACC ADJsonRpcProxy::get_jsonrpc_acc(struct json_object *new_obj) {
  char tmp_arr[256];
  new_obj = json_object_object_get(new_obj, "access");
  if (json_object_get_string(new_obj) == NULL)
    return CMD_TASK_ACCESS_SYNC;
  sprintf(tmp_arr, "%s", json_object_get_string(new_obj));
  printf("access : %s\n", tmp_arr);
  if (strcmp(tmp_arr, "sync") == 0)
    return CMD_TASK_ACCESS_SYNC;
  else if (strcmp(tmp_arr, "async") == 0)
    return CMD_TASK_ACCESS_ASYNC;
  else
    return CMD_TASK_ACCESS_SYNC;
}
int ADJsonRpcProxy::get_jsonrpc_id(struct json_object *new_obj, int *req_id) {
  char tmp_arr[256];
  new_obj = json_object_object_get(new_obj, "id");
  if (json_object_get_string(new_obj) == NULL)
    return -1;
  sprintf(tmp_arr, "%s", json_object_get_string(new_obj));
  *req_id = atoi(tmp_arr);
  return 0;
}
int ADJsonRpcProxy::json_send_error_response_string(
    int id, int sock_descriptor, JSON_RPC_ERR_TYPE err_type) {
  char message[AD_NET_SERVER_MAX_BUFFER_SIZE];
  json_object *intrnl_obj;
  json_object *error_obj;
  intrnl_obj = json_object_new_object();
  error_obj = json_object_new_object();
  json_object_object_add(error_obj, "code", json_object_new_int(err_type));
  switch (err_type) {
  case JSON_RPC_ERR_PARSE_ERROR:
    json_object_object_add(error_obj, "message",
                           json_object_new_string(JSON_RPC_PARSE_ERR_MSG));
    break;
  case JSON_RPC_ERR_INVALID_REQ:
    json_object_object_add(error_obj, "message",
                           json_object_new_string(JSON_RPC_INVALID_REQ_MSG));
    break;
  case JSON_RPC_ERR_METHOD_NOT_FOUND:
    json_object_object_add(
        error_obj, "message",
        json_object_new_string(JSON_RPC_METHOD_NOT_FOUND_MSG));
    break;
  case JSON_RPC_ERR_INVALID_PARAMS:
    json_object_object_add(error_obj, "message",
                           json_object_new_string(JSON_RPC_INVALID_PARAMS_MSG));
    break;
  default:
    json_object_object_add(error_obj, "message",
                           json_object_new_string(JSON_RPC_INTERNAL_ERR_MSG));
    break;
  }
  json_object_object_add(intrnl_obj, "jsonrpc",
                         json_object_new_string(JSON_RPC_VERSION));
  json_object_object_add(intrnl_obj, "error", error_obj);
  switch (err_type) {
  case JSON_RPC_ERR_METHOD_NOT_FOUND:
  case JSON_RPC_ERR_INVALID_PARAMS:
    json_object_object_add(intrnl_obj, "id", json_object_new_int(id));
    break;
  default:
    json_object_object_add(intrnl_obj, "id", NULL);
    break;
  }
  sprintf(message, "%s", json_object_to_json_string(intrnl_obj));
  ServerSocket.schedule_response(sock_descriptor, message, strlen(message));
  if (socketlog)
    printf("%s<--%s\n", get_timestamp(), message);
  json_object_put(intrnl_obj);
  json_object_put(error_obj);
  return 0;
}
int ADJsonRpcProxy::json_send_result_response_string(int id,
                                                     int sock_descriptor,
                                                     char *result,
                                                     api_task_obj *task_obj) {
  char message[AD_NET_SERVER_MAX_BUFFER_SIZE];
  json_object *intrnl_obj;
  json_object *tmp_obj = NULL;
  intrnl_obj = json_object_new_object();
  json_object_object_add(intrnl_obj, "jsonrpc",
                         json_object_new_string(JSON_RPC_VERSION));
  if (task_obj->json_resp_obj != NULL) {
    json_object *result_obj;
    result_obj = (json_object *)task_obj->json_resp_obj;
    json_object_object_add(intrnl_obj, "result", result_obj);
  } else {
    tmp_obj = json_object_new_object();
    json_object_object_add(tmp_obj, "return", json_object_new_string("fail"));
    json_object_object_add(tmp_obj, "errorType",
                           json_object_new_string(result));
    json_object_object_add(intrnl_obj, "result", tmp_obj);
  }
  json_object_object_add(intrnl_obj, "id", json_object_new_int(id));
  sprintf(message, "%s", json_object_to_json_string(intrnl_obj));
  ServerSocket.schedule_response(sock_descriptor, message, strlen(message));
  if (socketlog)
    printf("%s<--%s\n", get_timestamp(), message);
  if (tmp_obj != NULL)
    json_object_put(tmp_obj);
  json_object_put(intrnl_obj);
  return 0;
}
int ADJsonRpcProxy::json_single_string_response_sender(api_task_obj *resp_obj) {
  if (resp_obj->task_result != CMD_TASK_RESULT_SUCCESS)
    json_send_error_response_string(resp_obj->req_id,
                                    resp_obj->pNetData->sock_descriptor,
                                    JSON_RPC_ERR_INTERNAL_ERROR);
  if (resp_obj->json_resp_string_len > 0)
    json_send_result_response_string(resp_obj->req_id,
                                     resp_obj->pNetData->sock_descriptor,
                                     resp_obj->json_resp_string, resp_obj);
  else
    json_send_result_response_string(resp_obj->req_id,
                                     resp_obj->pNetData->sock_descriptor,
                                     (char *)JSON_RESULT_UNKNOWN, resp_obj);
  return 0;
}
int ADJsonRpcProxy::json_send_back_result(api_task_obj *resp_obj) {
  char result_string[32];
  if (resp_obj->task_result == CMD_TASK_RESULT_SUCCESS)
    sprintf(result_string, JSON_RESULT_SUCCESS);
  else if (resp_obj->task_result < 0)
    sprintf(result_string, JSON_RESULT_FAIL);
  else
    sprintf(result_string, JSON_RESULT_UNKNOWN);
  json_send_result_response_string(resp_obj->req_id,
                                   resp_obj->pNetData->sock_descriptor,
                                   result_string, resp_obj);
  return 0;
}
int ADJsonRpcProxy::json_process_request(net_data_obj *req_obj) {
  struct api_task_obj *pTaskObj = NULL;
  ADJsonRpcCommand *pRpcMethod = NULL;
  int json_req_id;
  struct json_object *new_obj = NULL;
  char *json_string = req_obj->data_buffer;
  if (json_string[0] == '[') {
    json_send_error_response_string(0, req_obj->sock_descriptor,
                                    JSON_RPC_ERR_INVALID_REQ);
    return -1;
  }
  if (json_checker_function(json_string) != 0) {
    if (socketlog)
      printf("%s-->%s\n", get_timestamp(), json_string);
    json_send_error_response_string(0, req_obj->sock_descriptor,
                                    JSON_RPC_ERR_PARSE_ERROR);
    return -1;
  }
  new_obj = json_tokener_parse(json_string);
  if (new_obj == NULL) {
    if (socketlog)
      printf("%s-->%s\n", get_timestamp(), json_string);
    json_send_error_response_string(0, req_obj->sock_descriptor,
                                    JSON_RPC_ERR_PARSE_ERROR);
    return -1;
  }
  if (check_jsonrpc_2_0(new_obj) == 0)
    ;
  else
    ;
  if (get_jsonrpc_id(new_obj, &json_req_id) != 0) {
    if (socketlog)
      printf("%s-->%s\n", get_timestamp(), json_string);
    json_send_error_response_string(0, req_obj->sock_descriptor,
                                    JSON_RPC_ERR_INVALID_REQ);
    json_object_put(new_obj);
    return -1;
  }
  pRpcMethod = get_jsonrpc_method(new_obj);
  if (pRpcMethod == NULL) {
    if (socketlog)
      printf("%s-->%s\n", get_timestamp(), json_string);
    json_send_error_response_string(json_req_id, req_obj->sock_descriptor,
                                    JSON_RPC_ERR_METHOD_NOT_FOUND);
    json_object_put(new_obj);
    return -1;
  }
  if (socketlog)
    printf("%s-->%s\n", get_timestamp(), json_string);
  OBJECT_MEM_NEW(pTaskObj, api_task_obj);
  if (pTaskObj == NULL) {
    json_send_error_response_string(json_req_id, req_obj->sock_descriptor,
                                    JSON_RPC_ERR_INTERNAL_ERROR);
    json_object_put(new_obj);
    return -1;
  }
  pTaskObj->ident = ReqRespChain.chain_generate_ident();
  pTaskObj->req_id = json_req_id;
  pTaskObj->pNetData = req_obj;
  pTaskObj->pRpcMethod = pRpcMethod;
  pTaskObj->access = get_jsonrpc_acc(new_obj);
  pTaskObj->task_result = CMD_TASK_RESULT_NOT_STARTED;
  pTaskObj->rpc_code = RPC_SRV_RESULT_NOT_STARTED;
  pTaskObj->result_code = -32603;
  pTaskObj->json_resp_obj = NULL;
  pTaskObj->json_resp_string = NULL;
  pTaskObj->json_resp_string_len = 0;
  pTaskObj->custom_result_string[0] = '\0';
  if (ReqRespChain.chain_put((void *)pTaskObj) != 0) {
    printf("failed! unable to push json-req-task-obj to chain!\n");
    json_send_error_response_string(json_req_id, req_obj->sock_descriptor,
                                    JSON_RPC_ERR_INTERNAL_ERROR);
    json_object_put(new_obj);
    OBJ_MEM_DELETE(pTaskObj);
    return -1;
  }
  pTaskObj->pRpcMethod->pParent->rpc_call_notification(pTaskObj);
  json_object_put(new_obj);
  total_req_received++;
  return 0;
}
int ADJsonRpcProxy::json_send_response(struct api_task_obj *pTaskObj) {
  int *pRespID = NULL;
  OBJECT_MEM_NEW(pRespID, int);
  if (pRespID == NULL) {
    return -1;
  }
  *pRespID = pTaskObj->ident;
  if (RespChain.chain_put((void *)pRespID) != 0) {
    printf("failed! unable to push responseID to chain!\n");
    OBJ_MEM_DELETE(pRespID);
    return -1;
  }
  RespThread.wakeup_thread();
  return 0;
}
int ADJsonRpcProxy::json_process_rsponse() {
  struct api_task_obj *pTaskObj = NULL;
  int *pRespID = NULL;
  pRespID = (int *)RespChain.chain_get();
  if (pRespID == NULL) {
    return -1;
  }
  pTaskObj = (api_task_obj *)ReqRespChain.chain_remove_by_ident(*pRespID);
  if (pTaskObj == NULL) {
    OBJ_MEM_DELETE(pRespID);
    return -1;
  }
  json_send_new_result(pTaskObj);
  if (pTaskObj->pNetData->data_buffer != NULL)
    MEM_DELETE(pTaskObj->pNetData->data_buffer);
  if (pTaskObj->pNetData != NULL)
    MEM_DELETE(pTaskObj->pNetData);
  if (pTaskObj->json_resp_obj != NULL) {
    ;
  }
  if (pTaskObj->json_resp_string != NULL)
    MEM_DELETE(pTaskObj->json_resp_string);
  OBJ_MEM_DELETE(pTaskObj);
  OBJ_MEM_DELETE(pRespID);
  total_res_sent++;
  return 0;
}
int ADJsonRpcProxy::json_send_new_result(api_task_obj *resp_obj) {
  char result_string[1100];
  switch (resp_obj->task_result) {
  case CMD_TASK_RESULT_FAIL:
    snprintf(result_string, sizeof(result_string), "%s",
             (char *)MSG_CMD_TASK_RESULT_FAIL);
    break;
  case CMD_TASK_RESULT_SUCCESS:
    snprintf(result_string, sizeof(result_string), "%s",
             (char *)MSG_CMD_TASK_RESULT_SUCCESS);
    break;
  case CMD_TASK_RESULT_INVALID_PARAM:
    snprintf(result_string, sizeof(result_string), "%s",
             (char *)MSG_CMD_TASK_RESULT_INVALID_PARAM);
    break;
  case CMD_TASK_RESULT_DEV_TIMEOUT:
    snprintf(result_string, sizeof(result_string), "%s",
             (char *)MSG_CMD_TASK_RESULT_DEV_TIMEOUT);
    break;
  case CMD_TASK_RESULT_DEV_ERR:
    snprintf(result_string, sizeof(result_string), "%s",
             (char *)MSG_CMD_TASK_RESULT_DEV_ERR);
    break;
  case CMD_TASK_RESULT_MEM_FAIL:
    snprintf(result_string, sizeof(result_string), "%s",
             (char *)MSG_CMD_TASK_RESULT_MEM_FAIL);
    break;
  case CMD_TASK_RESULT_IN_PROGRESS:
    snprintf(result_string, sizeof(result_string), "%s",
             (char *)MSG_CMD_TASK_RESULT_IN_PROGRESS);
    break;
  case CMD_TASK_RESULT_ACTUAL_VALUE:
    snprintf(result_string, sizeof(result_string), "%s",
             resp_obj->custom_result_string);
    break;
  case CMD_TASK_RESULT_UNKNOWN:
    snprintf(result_string, sizeof(result_string), "%s",
             (char *)MSG_CMD_TASK_RESULT_UNKNOWN);
    break;
  default:
    snprintf(result_string, sizeof(result_string), "%s",
             (char *)MSG_CMD_TASK_RESULT_UNKNOWN);
    break;
  }
  json_send_result_response_string(resp_obj->req_id,
                                   resp_obj->pNetData->sock_descriptor,
                                   result_string, resp_obj);
  return 0;
}
