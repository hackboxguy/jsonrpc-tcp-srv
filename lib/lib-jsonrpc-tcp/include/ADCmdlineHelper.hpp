#ifndef __ADCMDLINEHELPER_H_
#define __ADCMDLINEHELPER_H_
#include "ADCmnCltCmdProcessor.hpp"
#include "ADCmnDevTypes.h"
#include "ADGenericChain.hpp"
#include "ADJsonRpcClient.hpp"
#include "ADThreadedSockClient.hpp"
#include <getopt.h>
#define MAX_LONG_OPT_ITEMS 210
typedef struct OptionsEntryObj_t {
  struct option cmd_option;
  char cmd_name[1024];
} OptionsEntryObj;
typedef struct HelpInfoEntryObj_t {
  char *help_msg;
} HelpInfoEntryObj;
typedef struct IpAddrEntryObj_t {
  char ip_addr[255];
} IpAddrEntryObj;
typedef enum CMDLINE_HELPER_MODE_T {
  CMDLINE_HELPER_MODE_CLIENT = 0,
  CMDLINE_HELPER_MODE_SERVER,
  CMDLINE_HELPER_MODE_UNKNOWN,
  CMDLINE_HELPER_MODE_NONE,
} CMDLINE_HELPER_MODE;
typedef enum CMDLINE_OPT_TYPE_T {
  CMDLINE_OPT_TYPE_NO = 0,
  CMDLINE_OPT_TYPE_YES,
  CMDLINE_OPT_TYPE_UNKNOWN,
  CMDLINE_OPT_TYPE_NONE
} CMDLINE_OPT_TYPE;
#define CONSUMERS_OPTIONS_ARG_START_BOUNDARY 256
class ADCmdlineHelperProducer;
class ADCmdlineHelperConsumer {
public:
  virtual int parse_my_cmdline_options(int arg, char *sub_arg) = 0;
  virtual int run_my_commands(CmdExecutionObj *pCmdObj,
                              ADJsonRpcClient *pSrvSockConn,
                              ADGenericChain *pOutMsgList,
                              ADThreadedSockClientProducer *pWorker) = 0;
  virtual int run_my_autotest(char *ip, int interval_us, int max_loop,
                              int test_num) = 0;
  virtual int print_my_version() = 0;
  virtual int get_my_server_port() = 0;
  virtual ~ADCmdlineHelperConsumer(){};
};
class ADCmdlineHelperProducer {
  static int IDGenerator;
  ADCmdlineHelperConsumer *pConsumer;
  int id;

protected:
  int is_helper_attached(void) {
    if (pConsumer == NULL)
      return -1;
    return 0;
  }
  int parse_subscribers_cmdline(int arg, char *sub_arg) {
    if (pConsumer != NULL)
      return pConsumer->parse_my_cmdline_options(arg, sub_arg);
    return -1;
  }
  int run_subscribers_commands(CmdExecutionObj *pCmdObj,
                               ADJsonRpcClient *pSrvSockConn,
                               ADGenericChain *pOutMsgList,
                               ADThreadedSockClientProducer *pWorker) {
    if (pConsumer != NULL)
      return pConsumer->run_my_commands(pCmdObj, pSrvSockConn, pOutMsgList,
                                        pWorker);
    return -1;
  }
  int run_subscribers_autotest(char *ip, int interval_us, int max_loop,
                               int test_num) {
    if (pConsumer != NULL)
      return pConsumer->run_my_autotest(ip, interval_us, max_loop, test_num);
    return -1;
  }
  int print_subscribers_version() {
    if (pConsumer != NULL)
      return pConsumer->print_my_version();
    return -1;
  }
  int get_subscribers_server_port() {
    if (pConsumer != NULL)
      return pConsumer->get_my_server_port();
    return -1;
  }

public:
  ADCmdlineHelperProducer() {
    id = IDGenerator++;
    pConsumer = NULL;
  }
  virtual ~ADCmdlineHelperProducer(){};
  int attach_helper(ADCmdlineHelperConsumer *c) {
    if (pConsumer == NULL) {
      pConsumer = c;
      return id;
    } else
      return -1;
  }
  int getID() { return id; }
};
class ADCmdlineHelper : public ADCmdlineHelperProducer,
                        public ADChainConsumer,
                        public ADThreadedSockClientConsumer,
                        public ADCmnCltCmdProcessor {
  struct option long_options_table[MAX_LONG_OPT_ITEMS];
  char *complete_subargument;
  char *subarg_ptr;
  ADGenericChain OptionsChain, HelpMsgChain, ipChain, ClientWorkersList;
  int HelpMsgChainID;
  int help_printed;
  CMDLINE_HELPER_MODE my_mode;
  int autotest;
  int interval_us;
  int interval_ms_delay;
  int max_loop;
  int maxLogfileSize;
  int test_num;
  char ip[255];
  int port;
  CMDLINE_OPT_TYPE emulation_mode;
  CMDLINE_OPT_TYPE socket_log;
  CMDLINE_OPT_TYPE debug_log;
  char settings[1024];
  char short_options[255];
  ADCMN_DEV_INFO DeviceInfo;
  virtual int run_user_command(CmdExecutionObj *pCmdObj,
                               ADJsonRpcClient *pSrvSockConn,
                               ADGenericChain *pOutMsgList,
                               ADThreadedSockClientProducer *pWorker);
  virtual int identify_chain_element(void *element, int ident,
                                     ADChainProducer *pObj);
  virtual int double_identify_chain_element(void *element, int ident1,
                                            int ident2, ADChainProducer *pObj);
  virtual int free_chain_element_data(void *element, ADChainProducer *pObj);
  int Is_it_valid_file(char *filepath);
  int fill_long_options_table();
  int parse_ip_list_opt(char *subarg);
  int parse_port_number_opt(char *subarg);
  int parse_emulation_mode_opt(char *subarg);
  int parse_socket_log_opt(char *subarg);
  int parse_board_type_opt(char *subarg);
  int grep_ip_line(char *ip_filepath, int line, char *ip);
  int count_total_ip_from_file(char *ip_filepath);
  int push_ip_to_list_from_file(char *file);
  int push_ip_to_list(char *ip);
  int wait_for_client_workers_to_finish();
  int delete_client_workers();
  int start_new_client_worker(char *ip_addr, int port);

public:
  ADGenericChain CmdChain;
  ADCmdlineHelper(CMDLINE_HELPER_MODE cmdline_mode);
  ADCmdlineHelper();
  ~ADCmdlineHelper();
  int init_myself();
  int run_commands();
  int is_help_printed();
  int set_new_subargument(char *arg);
  int get_next_subargument(char **subarg);
  int parse_cmdline_arguments(int argc, char **argv);
  int insert_options_entry(char *opt_name, int has_arg, int val,
                           int my_own_options = 0);
  int insert_help_entry(char *HelpMsg);
  int print_help();
  int get_port_number();
  CMDLINE_OPT_TYPE get_emulation_mode();
  int get_ip_addr(char *addr);
  int get_loop_count();
  int get_interval_delay_ms();
  char *get_settings_file();
  int get_dev_info(ADCMN_DEV_INFO *pInfo);
  int push_int_type_command(int rpc_cmd, const char *get_rpc_name,
                            const char *set_rpc_name, char *subarg,
                            int readonly = 0);
  int push_action_type_noarg_command(int rpc_cmd, const char *rpc_name,
                                     char *ret_string = NULL);
  int push_single_int_get_set_command(int getcmd, int setcmd,
                                      const char *get_rpc_name,
                                      const char *set_rpc_name,
                                      char *param_name, char *subarg,
                                      int readonly = 0);
  int push_single_enum_get_set_command(int getcmd, int setcmd,
                                       const char *get_rpc_name,
                                       const char *set_rpc_name,
                                       const char **enum_string_table,
                                       int enum_max_val, char *param_name,
                                       char *subarg, char *ret_string = NULL);
  int push_int_get_set_with_dev_addr_arg_command(
      int getcmd, int setcmd, const char *get_rpc_name,
      const char *set_rpc_name, char *int_param_name, char *addr_param_name,
      int addr_param_val, char *subarg,
      RPC_SRV_ACT forced_act = RPC_SRV_ACT_READ_AND_WRITE);
  int push_string_get_set_with_dev_addr_arg_command(
      int getcmd, int setcmd, const char *get_rpc_name,
      const char *set_rpc_name, char *string_param_name, char *addr_param_name,
      int addr_param_val, char *subarg,
      RPC_SRV_ACT forced_act = RPC_SRV_ACT_READ_AND_WRITE);
  int push_string_get_set_command(
      int getcmd, int setcmd, const char *get_rpc_name,
      const char *set_rpc_name, char *string_param_name, char *subarg,
      RPC_SRV_ACT forced_act = RPC_SRV_ACT_READ_AND_WRITE);
  int push_string_get_set_with_string_arg_command(
      int getcmd, int setcmd, const char *get_rpc_name,
      const char *set_rpc_name, char *first_par_name, char *first_par_val,
      char *sec_para_name, char *sec_para_val, char *subarg,
      RPC_SRV_ACT forced_act = RPC_SRV_ACT_READ_AND_WRITE);
  int push_enum_get_set_with_dev_addr_arg_command(
      int getcmd, int setcmd, const char *get_rpc_name,
      const char *set_rpc_name, const char **enum_string_table,
      int enum_max_val, char *enum_param_name, char *addr_param_name,
      int addr_param_val, char *subarg,
      RPC_SRV_ACT forced_act = RPC_SRV_ACT_READ_AND_WRITE);
  int push_get_task_progress_command(char *get_rpc_name, char *taskID_name,
                                     char *taskSts_name, int command,
                                     char *subarg);
  int push_get_task_progress_command_with_dev_addr(char *get_rpc_name,
                                                   char *addr, int addr_val,
                                                   char *taskID_name,
                                                   char *taskSts_name,
                                                   int command, char *subarg);
  int push_string_get_set_with_enum_arg(
      int getcmd, int setcmd, const char *get_rpc, const char *set_rpc,
      const char *enum_name, int enum_max, const char **enum_table,
      const char *string_name, char *subarg,
      RPC_SRV_ACT forced_act = RPC_SRV_ACT_READ_AND_WRITE);
  int push_double_int_set_command(int setcmd, const char *set_rpc_name,
                                  char *param1_name, char *param2_name,
                                  char *subarg);
  int get_socket_log_opt();
  bool get_debug_log_opt();
};
#endif
