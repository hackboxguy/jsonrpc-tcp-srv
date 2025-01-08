#ifndef __SYSMGRCLT_CMDLINE_H_
#define __SYSMGRCLT_CMDLINE_H_
#include "ADCmdlineHelper.hpp"
#include "ADCmnStringProcessor.hpp"
#include "SysmgrJsonDef.h"
class SysmgrCltCmdline : public ADCmdlineHelperConsumer,
                         public ADCmnStringProcessor {
  ADCmdlineHelper CmdlineHelper;

  // Chain-callback functions
  virtual int parse_my_cmdline_options(int arg, char *sub_arg);
  virtual int
  run_my_commands(CmdExecutionObj *pCmdObj, ADJsonRpcClient *pSrvSockConn,
                  ADGenericChain *pOutMsgList,
                  ADThreadedSockClientProducer *pWorker); // char* ip);
  virtual int run_my_autotest(char *ip, int interval_us, int max_loop,
                              int test_num);
  virtual int print_my_version();
  virtual int get_my_server_port();
  int push_get_string_of_index(char *subarg, char *rpc_name, int rpc_index,
                               char *arg_name, char *result_name);
  int run_get_string_of_index(CmdExecutionObj *pCmdObj,
                              ADJsonRpcClient *pSrvSockConn,
                              ADGenericChain *pOutMsgList,
                              ADThreadedSockClientProducer *pWorker);

  int push_fmw_version_read_command(char *subarg);
  int run_fmw_version_command(CmdExecutionObj *pCmdObj,
                              ADJsonRpcClient *pSrvSockConn,
                              ADGenericChain *pOutMsgList,
                              ADThreadedSockClientProducer *pWorker);
  int push_get_info_command(int cmd, char *rpc_name, char *arg1_name,
                            char *arg2_name, char *arg3_name);
  int run_get_info_command(CmdExecutionObj *pCmdObj,
                           ADJsonRpcClient *pSrvSockConn,
                           ADGenericChain *pOutMsgList,
                           ADThreadedSockClientProducer *pWorker);

  int push_fmw_update_command(char *subarg);
  int run_fmw_update_command(CmdExecutionObj *pCmdObj,
                             ADJsonRpcClient *pSrvSockConn,
                             ADGenericChain *pOutMsgList,
                             ADThreadedSockClientProducer *pWorker);

  int push_file_download(char *subarg, EJSON_SYSMGR_RPC_TYPES rpc_cmd);
  int run_file_download(CmdExecutionObj *pCmdObj, ADJsonRpcClient *pSrvSockConn,
                        ADGenericChain *pOutMsgList,
                        ADThreadedSockClientProducer *pWorker);
  int push_get_indexed_msg_command(char *subarg, char *rpc_name, int rpc_index,
                                   char *arg_name, char *result_name);
  int run_get_indexed_msg_command(CmdExecutionObj *pCmdObj,
                                  ADJsonRpcClient *pSrvSockConn,
                                  ADGenericChain *pOutMsgList,
                                  ADThreadedSockClientProducer *pWorker);

  int push_shell_cmd(char *subarg, EJSON_SYSMGR_RPC_TYPES cmd, char *cmdname);
  int run_shell_cmd(CmdExecutionObj *pCmdObj, ADJsonRpcClient *pSrvSockConn,
                    ADGenericChain *pOutMsgList,
                    ADThreadedSockClientProducer *pWorker);

public:
  SysmgrCltCmdline();
  ~SysmgrCltCmdline();
  int parse_cmdline_arguments(int argc, char **argv);
};
#endif
