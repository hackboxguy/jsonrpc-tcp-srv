#ifndef __AD_CMN_CLT_CMD_PROCESSOR_H_
#define __AD_CMN_CLT_CMD_PROCESSOR_H_
#include "ADCmnStringProcessor.hpp"
#include "ADCommon.hpp"
#include "ADGenericChain.hpp"
#include "ADJsonRpcClient.hpp"
#include "JsonCmnDef.h"
#define CLIENT_CMD_RESULT_FAIL "failed!!!"
#define CLIENT_CMD_RESULT_SUCCESS "success"
#define CLIENT_CMD_RESULT_INVALID_ACT "invalid action!!!"
typedef enum CLIENT_CMD_TYPE_T {
  CLIENT_CMD_TYPE_ACTION_NO_ARG = 0,
  CLIENT_CMD_TYPE_ENUM_GET_SET,
  CLIENT_CMD_TYPE_INT_GET_SET,
  CLIENT_CMD_TYPE_INT_GET_SET_WITH_DEV_ADDR,
  CLIENT_CMD_TYPE_ENUM_GET_SET_WITH_DEV_ADDR,
  CLIENT_CMD_TYPE_STRING_GET_SET_WITH_DEV_ADDR,
  CLIENT_CMD_TYPE_STRING_GET_SET_WITH_STRING_PARA,
  CLIENT_CMD_TYPE_STRING_GET_SET_WITH_ENUM_PARA,
  CLIENT_CMD_TYPE_GET_TASK_STATUS,
  CLIENT_CMD_TYPE_STRING_GET_SET,
  CLIENT_CMD_TYPE_USER_DEFINED,
  CLIENT_CMD_TYPE_GET_TASK_STATUS_WITH_DEV_ADDR,
  CLIENT_CMD_TYPE_DOUBLE_INT_SET,
  CLIENT_CMD_TYPE_UNKNOWN,
  CLIENT_CMD_TYPE_NONE
} CLIENT_CMD_TYPE;
typedef struct CmdExecutionObj_t {
  int command;
  CLIENT_CMD_TYPE cmd_type;
  RPC_SRV_ACT action;
  char get_rpc_name[JSON_RPC_METHOD_NAME_MAX_LENGTH];
  char set_rpc_name[JSON_RPC_METHOD_NAME_MAX_LENGTH];
  int cmd_int_val;
  int cmd_enum_val;
  int arg_param_int_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  double arg_param_ulong_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char first_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char first_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int first_arg_param_int_value;
  double first_arg_param_double_value;
  unsigned long first_arg_param_ulong_value;
  char second_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char second_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int second_arg_param_int_value;
  double second_arg_param_double_value;
  unsigned long second_arg_param_ulong_value;
  char third_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char third_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int third_arg_param_int_value;
  double third_arg_param_double_value;
  unsigned long third_arg_param_ulong_value;
  char fourth_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char fourth_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int fourth_arg_param_int_value;
  double fourth_arg_param_double_value;
  unsigned long fourth_arg_param_ulong_value;
  char fifth_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char fifth_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int fifth_arg_param_int_value;
  double fifth_arg_param_double_value;
  char sixth_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char sixth_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int sixth_arg_param_int_value;
  double sixth_arg_param_double_value;
  char seventh_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char seventh_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int seventh_arg_param_int_value;
  double seventh_arg_param_double_value;
  char eighth_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char eighth_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int eighth_arg_param_int_value;
  double eighth_arg_param_double_value;
  char ninth_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char ninth_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int ninth_arg_param_int_value;
  double ninth_arg_param_double_value;
  char tenth_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char tenth_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int tenth_arg_param_int_value;
  double tenth_arg_param_double_value;
  char eleventh_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char eleventh_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int eleventh_arg_param_int_value;
  double eleventh_arg_param_double_value;
  char twelfth_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char twelfth_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int twelfth_arg_param_int_value;
  double twelfth_arg_param_double_value;
  char thirteen_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char thirteen_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int thirteen_arg_param_int_value;
  double thirteen_arg_param_double_value;
  char fourteen_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char fourteen_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int fourteen_arg_param_int_value;
  char fifteen_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char fifteen_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  int fifteen_arg_param_int_value;
  char sixteen_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char sixteen_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  float sixteen_arg_param_float_value;
  char seventeen_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char seventeen_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  float seventeen_arg_param_float_value;
  char eighteen_arg_param_name[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char eighteen_arg_param_value[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  float eighteen_arg_param_float_value;
  RPC_SRV_RESULT result;
  char rpc_resp_result[JSON_RPC_METHOD_RESP_MAX_LENGTH];
} CmdExecutionObj;
typedef struct OutputPrintMsgObj_t {
  string ip;
  char rpc_method_name[JSON_RPC_METHOD_NAME_MAX_LENGTH];
  RPC_SRV_ACT action;
  char result[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  char result_parameter[JSON_RPC_METHOD_RESP_MAX_LENGTH];
  double exec_time;
} OutputPrintMsgObj;
class ADCmnCltCmdProcessor : public ADCmnStringProcessor {
public:
  ADCmnCltCmdProcessor();
  ~ADCmnCltCmdProcessor();
  int copy_cmd_exec_obj(CmdExecutionObj *pCmdObjDest,
                        CmdExecutionObj *pCmdObjSrc);
  int run_cmd_type_enum_get_set(CmdExecutionObj *pCmdObj,
                                ADJsonRpcClient *pSrvSockConn,
                                ADGenericChain *pOutMsgList);
  int run_cmd_type_int_get_set(CmdExecutionObj *pCmdObj,
                               ADJsonRpcClient *pSrvSockConn,
                               ADGenericChain *pOutMsgList);
  int run_cmd_type_int_get_set_with_dev_addr(CmdExecutionObj *pCmdObj,
                                             ADJsonRpcClient *pSrvSockConn,
                                             ADGenericChain *pOutMsgList);
  int run_cmd_type_enum_get_set_with_dev_addr(CmdExecutionObj *pCmdObj,
                                              ADJsonRpcClient *pSrvSockConn,
                                              ADGenericChain *pOutMsgList);
  int run_get_task_progress_command(CmdExecutionObj *pCmdObj,
                                    ADJsonRpcClient *pSrvSockConn,
                                    ADGenericChain *pOutMsgList);
  int run_cmd_type_string_get_set_with_dev_addr(CmdExecutionObj *pCmdObj,
                                                ADJsonRpcClient *pSrvSockConn,
                                                ADGenericChain *pOutMsgList);
  int run_cmd_type_string_get_set(CmdExecutionObj *pCmdObj,
                                  ADJsonRpcClient *pSrvSockConn,
                                  ADGenericChain *pOutMsgList);
  int run_get_task_progress_command_with_dev_addr(CmdExecutionObj *pCmdObj,
                                                  ADJsonRpcClient *pSrvSockConn,
                                                  ADGenericChain *pOutMsgList);
  int run_cmd_type_action_noarg(CmdExecutionObj *pCmdObj,
                                ADJsonRpcClient *pSrvSockConn,
                                ADGenericChain *pOutMsgList);
  int run_cmd_type_string_get_set_with_enum_para(CmdExecutionObj *pCmdObj,
                                                 ADJsonRpcClient *pSrvSockConn,
                                                 ADGenericChain *pOutMsgList);
  int log_print_message(ADJsonRpcClient *pSrvSockConn, char *rpc_name,
                        RPC_SRV_ACT action, char *result,
                        ADGenericChain *pOutMsgList);
  int log_print_message(ADJsonRpcClient *pSrvSockConn, char *rpc_method_name,
                        RPC_SRV_ACT action, RPC_SRV_RESULT result,
                        ADGenericChain *pOutMsgList, char *result_val);
  int run_cmd_type_double_int_set(CmdExecutionObj *pCmdObj,
                                  ADJsonRpcClient *pSrvSockConn,
                                  ADGenericChain *pOutMsgList);
};
#endif
