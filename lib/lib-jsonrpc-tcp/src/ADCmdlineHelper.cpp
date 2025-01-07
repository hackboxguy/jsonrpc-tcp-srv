#include "ADCmdlineHelper.hpp"
#include "ADJsonRpcMgr.hpp"
#include <iostream>
#include <stdio.h>
#include <string.h>
using namespace std;
int ADCmdlineHelperProducer::IDGenerator = 0;
ADCmdlineHelper::ADCmdlineHelper(CMDLINE_HELPER_MODE cmdline_mode) {
  my_mode = cmdline_mode;
  init_myself();
}
ADCmdlineHelper::ADCmdlineHelper() {
  my_mode = CMDLINE_HELPER_MODE_CLIENT;
  init_myself();
}
ADCmdlineHelper::~ADCmdlineHelper() {}
int ADCmdlineHelper::init_myself() {
  CmdChain.attach_helper(this);
  OptionsChain.attach_helper(this);
  HelpMsgChainID = HelpMsgChain.attach_helper(this);
  ipChain.attach_helper(this);
  ClientWorkersList.attach_helper(this);
  complete_subargument = NULL;
  subarg_ptr = NULL;
  help_printed = 0;
  autotest = 0;
  interval_us = 1000;
  interval_ms_delay = 1;
  max_loop = 1;
  maxLogfileSize = -1;
  test_num = 0;
  strcpy(ip, "127.0.0.1");
  port = -1;
  emulation_mode = CMDLINE_OPT_TYPE_NO;
  socket_log = CMDLINE_OPT_TYPE_NO;
  debug_log = CMDLINE_OPT_TYPE_NO;
  settings[0] = '\0';
  strcpy(short_options, "hvpiegtdlnskwrabxzucfjmoyq1");
  insert_options_entry((char *)"help", optional_argument, 'h', 1);
  insert_options_entry((char *)"version", no_argument, 'v', 1);
  insert_help_entry((char *)"--version                  (print the version "
                            "number of this tool)");
  insert_options_entry((char *)"port", optional_argument, 'p', 1);
  insert_help_entry((
      char *)"--port                     (server's listening tcp port number)");
  if (my_mode == CMDLINE_HELPER_MODE_CLIENT) {
    insert_options_entry((char *)"ip", optional_argument, 'i', 1);
    insert_help_entry(
        (char *)"--ip=ip1,ip2,ip3,ipN       (server ip address: default is "
                "localhost, multiple ip's can be specified)");
    insert_options_entry((char *)"autotest", optional_argument, 't', 1);
    insert_options_entry((char *)"delay", optional_argument, 'd', 1);
    insert_help_entry((char *)"--delay                    (delay between loop "
                              "iteration: default is 1ms)");
    insert_options_entry((char *)"loopcount", optional_argument, 'l', 1);
    insert_help_entry((char *)"--loopcount                (repeatation of a "
                              "test: default is 1)");
    insert_options_entry((char *)"testnum", optional_argument, 'n', 1);
    insert_options_entry((char *)"tasksts", optional_argument, 'k', 1);
    insert_help_entry((char *)"--tasksts=taskID           (read the status of "
                              "the task which was in progress)");
    insert_options_entry((char *)"shutdown", optional_argument, 'w', 1);
    insert_help_entry(
        (char *)"--shutdown                 (shuts down the service)");
    insert_options_entry((char *)"serversion", optional_argument, 'r', 1);
    insert_help_entry(
        (char *)"--serversion               (read the rpc server version)");
    insert_options_entry((char *)"savedata", optional_argument, 'a', 1);
    insert_help_entry((
        char
            *)"--savedata                 (trigger saving of server settings)");
    insert_options_entry((char *)"datasts", optional_argument, 'b', 1);
    insert_help_entry((char *)"--datasts                  (read the status of "
                              "settings save/load action)");
    insert_options_entry((char *)"rsttasksts", optional_argument, 'x', 1);
    insert_help_entry((char *)"--rsttasksts               (resets task status "
                              "chain and ID to 0)");
    insert_options_entry((char *)"readysts", optional_argument, 'z', 1);
    insert_help_entry(
        (char *)"--readysts                 (read the ready state of service)");
    insert_options_entry((char *)"logdebug", optional_argument, 'c', 1);
    insert_help_entry((char *)"--logdebug=[enable/disable](read/write debug "
                              "logging status of the service)");
    insert_options_entry((char *)"storefactory", optional_argument, 'j', 1);
    insert_help_entry((char *)"--storefactory             (trigger storing of "
                              "factory settings)");
    insert_options_entry((char *)"restorefactory", optional_argument, 'm', 1);
    insert_help_entry((char *)"--restorefactory           (trigger restoring "
                              "of factory settings)");
    insert_options_entry((char *)"evSub", optional_argument, 'o', 1);
    insert_help_entry((
        char *)"--evSub=cltToken,portNum   (subscribe for event notification)");
    insert_options_entry((char *)"evUnsub", optional_argument, 'y', 1);
    insert_help_entry((char *)"--evUnsub=srvToken         (unsubscribe for "
                              "event notification)");
    insert_options_entry((char *)"evNotify", optional_argument, 'q', 1);
    insert_help_entry((char *)"--evNotify=evntNum,evntArg (notify event)");
    insert_options_entry((char *)"srvop", optional_argument, '1', 1);
    insert_help_entry((char *)"--srvop=[on/off/idle/reboot/idlenoexthw](read/"
                              "write device operation state of the service)");
    insert_options_entry((char *)"rwbyte", optional_argument, '2', 1);
    insert_help_entry((char *)"--rwbyte=addr,data         (read/write "
                              "middleware byte from/to given address)");
    insert_options_entry((char *)"rwword", optional_argument, '3', 1);
    insert_help_entry((char *)"--rwword=addr,data         (read/write "
                              "middleware word from/to given address)");
    insert_options_entry((char *)"rwdword", optional_argument, '4', 1);
    insert_help_entry((char *)"--rwdword=addr,data        (read/write "
                              "middleware dword from/to given address)");
  } else if (my_mode == CMDLINE_HELPER_MODE_SERVER) {
    insert_options_entry((char *)"debuglog", no_argument, 'u', 1);
    insert_help_entry(
        (char *)"--debuglog                 (enable debug logging)");
    insert_options_entry((char *)"emulation", optional_argument, 'e', 1);
    insert_help_entry((char *)"--emulation                (disable actual h/w "
                              "access, run in emulation mode)");
    insert_options_entry((char *)"logsocket", optional_argument, 'g', 1);
    insert_help_entry((char *)"--logsocket                (enable json "
                              "req/resp string-print on socket)");
    insert_options_entry((char *)"settings", optional_argument, 's', 1);
    insert_help_entry((char *)"--settings=filepath        (settings text "
                              "filepath to be loaded and stored)");
    insert_options_entry((char *)"boardtype", optional_argument, 'f', 1);
    insert_help_entry(
        (char *)"--boardtype=[RASPI_0/RASPI_0W,RASPI_A/RASPI_APLUS/RASPI_B/"
                "RASPI_BPLUS/RASPI_B2/BAYTRAIL/BAYTRAIL_I210] (pass cpu board "
                "type to service)");
  }
  return 0;
}
int ADCmdlineHelper::identify_chain_element(void *element, int ident,
                                            ADChainProducer *pObj) {
  return -1;
}
int ADCmdlineHelper::double_identify_chain_element(void *element, int ident1,
                                                   int ident2,
                                                   ADChainProducer *pObj) {
  return -1;
}
int ADCmdlineHelper::free_chain_element_data(void *element,
                                             ADChainProducer *pObj) {
  if (pObj->getID() == HelpMsgChainID) {
    HelpInfoEntryObj *pEntry;
    pEntry = (HelpInfoEntryObj *)element;
    if (pEntry->help_msg != NULL)
      MEM_DELETE(pEntry->help_msg);
  }
  return 0;
}
int ADCmdlineHelper::is_help_printed() { return help_printed; }
int ADCmdlineHelper::set_new_subargument(char *arg) {
  int subarg_num = 0;
  if (complete_subargument != NULL) {
    free(complete_subargument);
    complete_subargument = NULL;
    subarg_ptr = NULL;
  }
  if ((arg == NULL) || (strlen(arg) <= 0)) {
    return subarg_num;
  }
  complete_subargument = strdup(arg);
  subarg_ptr = complete_subargument;
  subarg_num++;
  while (subarg_ptr != NULL) {
    subarg_ptr = strchr(subarg_ptr, ',');
    if (subarg_ptr != NULL) {
      subarg_num++;
      subarg_ptr = subarg_ptr + 1;
    }
  }
  subarg_ptr = complete_subargument;
  return subarg_num;
}
int ADCmdlineHelper::get_next_subargument(char **subarg) {
  char *c;
  static char *str = NULL;
  if (str != NULL) {
    free(str);
    str = NULL;
  }
  if (complete_subargument == NULL) {
    *subarg = NULL;
    return 0;
  }
  if (strlen(complete_subargument) <= 0) {
    *subarg = NULL;
    return 0;
  }
  if (subarg_ptr == NULL) {
    *subarg = NULL;
    return 0;
  }
  c = strchr(subarg_ptr, ',');
  if (c == NULL) {
    str = strdup(subarg_ptr);
    subarg_ptr = NULL;
  } else {
    *c = '\0';
    str = strdup(subarg_ptr);
    subarg_ptr = (c + 1);
  }
  *subarg = str;
  if (subarg_ptr == NULL) {
    return 1;
  } else {
    return 2;
  }
}
int ADCmdlineHelper::parse_cmdline_arguments(int argc, char **argv) {
  int arg;
  char *subarg;
  if (fill_long_options_table() != 0)
    return -1;
  do {
    arg = getopt_long(argc, argv, short_options, long_options_table, 0);
    if (arg == -1) {
      break;
    }
    set_new_subargument(optarg);
    switch (arg) {
    case '?':
      print_help();
      return 0;
      break;
    case 'h':
      print_help();
      return 0;
      break;
    case 'v':
      print_subscribers_version();
      help_printed = 1;
      break;
    case 'p':
      parse_port_number_opt(subarg);
      break;
    case 'u':
      debug_log = CMDLINE_OPT_TYPE_YES;
      break;
    case 'i':
      parse_ip_list_opt(subarg);
      break;
    case 'e':
      emulation_mode = CMDLINE_OPT_TYPE_YES;
      break;
    case 'g':
      socket_log = CMDLINE_OPT_TYPE_YES;
      break;
    case 't':
      autotest = 1;
      break;
    case 'd':
      if (get_next_subargument(&subarg) != 0)
        interval_ms_delay = atoi(subarg);
      break;
    case 'l':
      if (get_next_subargument(&subarg) != 0)
        max_loop = atoi(subarg);
      break;
    case 'n':
      if (get_next_subargument(&subarg) != 0)
        test_num = atoi(subarg);
      break;
    case 's':
      if (get_next_subargument(&subarg) != 0)
        strcpy(settings, subarg);
      break;
    case 'k':
      push_get_task_progress_command((char *)ADLIB_RPC_NAME_GET_TASK_STATUS,
                                     (char *)ADLIB_RPC_PARM_TASK_STS_ID,
                                     (char *)ADLIB_RPC_PARM_TASK_STS,
                                     ADLIB_RPC_INDX_GET_TASK_STATUS, subarg);
      break;
    case 'c': {
      const char *table[] = RPCMGR_RPC_DEBUG_LOG_ARGSTS_TBL;
      push_single_enum_get_set_command(
          EJSON_RPCGMGR_GET_DEBUG_LOG, EJSON_RPCGMGR_SET_DEBUG_LOG,
          RPCMGR_RPC_DEBUG_LOG_GET, RPCMGR_RPC_DEBUG_LOG_SET, &table[0],
          EJSON_RPCGMGR_FLAG_STATE_UNKNOWN, (char *)RPCMGR_RPC_DEBUG_LOG_ARGSTS,
          subarg);
    } break;
    case 'w':
      push_action_type_noarg_command(ADLIB_RPC_INDX_SHUTDOWN_SERVICE,
                                     ADLIB_RPC_NAME_SHUTDOWN_SERVICE,
                                     (char *)ADLIB_RPC_PARM_TASK_STS_ID);
      break;
    case 'r':
      push_single_int_get_set_command(
          ADLIB_RPC_INDX_GET_SRV_VER, ADLIB_RPC_INDX_GET_SRV_VER,
          ADLIB_RPC_NAME_GET_SRV_VER, ADLIB_RPC_NAME_GET_SRV_VER,
          (char *)ADLIB_RPC_PARM_GET_SRV_VER, subarg, 1);
      break;
    case 'a':
      push_action_type_noarg_command(ADLIB_RPC_INDX_TRIGGER_DATASAVE,
                                     (char *)ADLIB_RPC_NAME_TRIGGER_DATASAVE,
                                     (char *)ADLIB_RPC_PARM_TASK_STS_ID);
      break;
    case 'b':
      push_string_get_set_command(ADLIB_RPC_INDX_GET_SETTINGS_STATUS,
                                  ADLIB_RPC_INDX_GET_SETTINGS_STATUS,
                                  ADLIB_RPC_NAME_GET_SETTINGS_STATUS,
                                  ADLIB_RPC_NAME_GET_SETTINGS_STATUS,
                                  (char *)ADLIB_RPC_PARM_GET_SETTINGS_STATUS,
                                  subarg, RPC_SRV_ACT_READONLY);
      break;
    case 'x':
      push_action_type_noarg_command(ADLIB_RPC_INDX_RESET_TASK_STS,
                                     (char *)ADLIB_RPC_NAME_RESET_TASK_STS);
      break;
    case 'z':
      push_string_get_set_command(
          ADLIB_RPC_INDX_GET_READY_STS, ADLIB_RPC_INDX_GET_READY_STS,
          ADLIB_RPC_NAME_GET_READY_STS, ADLIB_RPC_NAME_GET_READY_STS,
          (char *)ADLIB_RPC_PARM_GET_READY_STS, subarg, RPC_SRV_ACT_READONLY);
      break;
    case 'f':
      parse_board_type_opt(subarg);
      break;
    case 'j':
      push_action_type_noarg_command(
          ADLIB_RPC_INDX_TRIGGER_STORE_FACTRY,
          (char *)ADLIB_RPC_NAME_TRIGGER_STORE_FACTRY,
          (char *)ADLIB_RPC_PARM_TASK_STS_ID);
      break;
    case 'm':
      push_action_type_noarg_command(
          ADLIB_RPC_INDX_TRIGGER_RESTORE_FACTRY,
          (char *)ADLIB_RPC_NAME_TRIGGER_RESTORE_FACTRY,
          (char *)ADLIB_RPC_PARM_TASK_STS_ID);
      break;
    case 'o':
      push_double_int_set_command(EJSON_RPCGMGR_EVENT_SUBSCRIBE,
                                  RPCMGR_RPC_EVENT_SUBSCRIBE,
                                  (char *)RPCMGR_RPC_EVENT_ARG_CLTTOK,
                                  (char *)RPCMGR_RPC_EVENT_ARG_PORT, subarg);
      break;
    case 'y':
      push_single_int_get_set_command(
          EJSON_RPCGMGR_EVENT_UNSUBSCRIBE, EJSON_RPCGMGR_EVENT_UNSUBSCRIBE,
          RPCMGR_RPC_EVENT_UNSUBSCRIBE, RPCMGR_RPC_EVENT_UNSUBSCRIBE,
          (char *)RPCMGR_RPC_EVENT_ARG_SRVTOK, subarg, 0);
      break;
    case 'q':
      push_double_int_set_command(EJSON_RPCGMGR_EVENT_NOTIFY,
                                  RPCMGR_RPC_EVENT_NOTIFY,
                                  (char *)RPCMGR_RPC_EVENT_ARG_EVENTNUM,
                                  (char *)RPCMGR_RPC_EVENT_ARG_EXTRA, subarg);
      break;
    case '1': {
      const char *table[] = RPCMGR_RPC_DEVOP_STATE_ARGSTS_TBL;
      push_single_enum_get_set_command(
          EJSON_RPCMGR_GET_DEVOP_STATE, EJSON_RPCMGR_SET_DEVOP_STATE,
          RPCMGR_RPC_DEVOP_STATE_GET, RPCMGR_RPC_DEVOP_STATE_SET, &table[0],
          EJSON_RPCGMGR_DEVOP_STATE_UNKNOWN,
          (char *)RPCMGR_RPC_DEVOP_STATE_ARGSTS, subarg);
    } break;
    case '2': {
      push_int_get_set_with_dev_addr_arg_command(
          EJSON_RPCMGR_GET_MW_BYTE, EJSON_RPCMGR_SET_MW_BYTE,
          RPCMGR_RPC_MW_BYTE_GET, RPCMGR_RPC_MW_BYTE_SET,
          (char *)RPCMGR_RPC_MW_ARGDATA, (char *)RPCMGR_RPC_MW_ARGADDR, -1,
          subarg);
    } break;
    case '3': {
      push_int_get_set_with_dev_addr_arg_command(
          EJSON_RPCMGR_GET_MW_WORD, EJSON_RPCMGR_SET_MW_WORD,
          RPCMGR_RPC_MW_WORD_GET, RPCMGR_RPC_MW_WORD_SET,
          (char *)RPCMGR_RPC_MW_ARGDATA, (char *)RPCMGR_RPC_MW_ARGADDR, -1,
          subarg);
    } break;
    case '4': {
      push_int_get_set_with_dev_addr_arg_command(
          EJSON_RPCMGR_GET_MW_DWORD, EJSON_RPCMGR_SET_MW_DWORD,
          RPCMGR_RPC_MW_DWORD_GET, RPCMGR_RPC_MW_DWORD_SET,
          (char *)RPCMGR_RPC_MW_ARGDATA, (char *)RPCMGR_RPC_MW_ARGADDR, -1,
          subarg);
    } break;
    default:
      if (parse_subscribers_cmdline(
              (arg - CONSUMERS_OPTIONS_ARG_START_BOUNDARY), subarg) != 0)
        return -1;
      break;
    }
  } while (arg != -1);
  if (port <= 0)
    port = get_subscribers_server_port();
  if (my_mode == CMDLINE_HELPER_MODE_CLIENT) {
    interval_us *= interval_ms_delay;
    if (autotest == 0) {
      run_commands();
    } else
      run_subscribers_autotest(ip, interval_us, max_loop, test_num);
  } else {
    ;
  }
  return 0;
}
int ADCmdlineHelper::get_port_number() { return port; }
int ADCmdlineHelper::get_ip_addr(char *addr) {
  strcpy(addr, ip);
  return 0;
}
CMDLINE_OPT_TYPE ADCmdlineHelper::get_emulation_mode() {
  return emulation_mode;
}
int ADCmdlineHelper::get_loop_count() { return max_loop; }
int ADCmdlineHelper::get_interval_delay_ms() { return interval_ms_delay; }
char *ADCmdlineHelper::get_settings_file() { return settings; }
int ADCmdlineHelper::fill_long_options_table() {
  int i = 0;
  OptionsEntryObj *pEntry;
  int total_items = OptionsChain.get_chain_size();
  if (total_items == 0)
    return -1;
  if (total_items >= MAX_LONG_OPT_ITEMS)
    return -1;
  for (i = 0; i < total_items; i++) {
    pEntry = (OptionsEntryObj *)OptionsChain.chain_get_by_index(i);
    if (pEntry != NULL) {
      long_options_table[i].name = pEntry->cmd_option.name;
      long_options_table[i].has_arg = pEntry->cmd_option.has_arg;
      long_options_table[i].flag = pEntry->cmd_option.flag;
      long_options_table[i].val = pEntry->cmd_option.val;
    }
  }
  long_options_table[i].name = 0;
  long_options_table[i].has_arg = 0;
  long_options_table[i].flag = 0;
  long_options_table[i].val = 0;
  return 0;
}
int ADCmdlineHelper::insert_options_entry(char *opt_name, int has_arg, int val,
                                          int my_own_options) {
  OptionsEntryObj *pEntry = NULL;
  OBJECT_MEM_NEW(pEntry, OptionsEntryObj);
  if (pEntry == NULL)
    return -1;
  if (my_own_options == 0)
    val = val + CONSUMERS_OPTIONS_ARG_START_BOUNDARY;
  strcpy(pEntry->cmd_name, opt_name);
  pEntry->cmd_option.name = pEntry->cmd_name;
  pEntry->cmd_option.has_arg = has_arg;
  pEntry->cmd_option.flag = NULL;
  pEntry->cmd_option.val = val;
  if (OptionsChain.chain_put((void *)pEntry) != 0) {
    printf("failed! unable to push options entry to chain!\n");
    OBJ_MEM_DELETE(pEntry);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::insert_help_entry(char *HelpMsg) {
  HelpInfoEntryObj *pEntry = NULL;
  OBJECT_MEM_NEW(pEntry, HelpInfoEntryObj);
  if (pEntry == NULL)
    return -1;
  pEntry->help_msg = NULL;
  ARRAY_MEM_NEW(pEntry->help_msg, strlen(HelpMsg) + 1);
  if (pEntry->help_msg == NULL) {
    printf("failed! unable to allocate memory for HelpMsg!\n");
    OBJ_MEM_DELETE(pEntry);
    return -1;
  }
  strcpy(pEntry->help_msg, HelpMsg);
  if (HelpMsgChain.chain_put((void *)pEntry) != 0) {
    printf("failed! unable to push options entry to chain!\n");
    OBJ_MEM_DELETE(pEntry->help_msg);
    OBJ_MEM_DELETE(pEntry);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::print_help() {
  int i = 0;
  HelpInfoEntryObj *pEntry;
  int total_items = HelpMsgChain.get_chain_size();
  if (total_items == 0)
    return -1;
  for (i = 0; i < total_items; i++) {
    pEntry = (HelpInfoEntryObj *)HelpMsgChain.chain_get_by_index(i);
    if (pEntry != NULL) {
      printf("%s", pEntry->help_msg);
      printf("\n");
    }
  }
  help_printed = 1;
  return 0;
}
int ADCmdlineHelper::push_int_type_command(int rpc_cmd,
                                           const char *get_rpc_name,
                                           const char *set_rpc_name,
                                           char *subarg, int readonly) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  pCmdObj->command = rpc_cmd;
  strcpy(pCmdObj->get_rpc_name, get_rpc_name);
  strcpy(pCmdObj->set_rpc_name, set_rpc_name);
  if (readonly)
    pCmdObj->action = RPC_SRV_ACT_READ;
  else if (get_next_subargument(&subarg) == 0) {
    pCmdObj->action = RPC_SRV_ACT_READ;
  } else {
    pCmdObj->cmd_int_val = atoi(subarg);
    pCmdObj->action = RPC_SRV_ACT_WRITE;
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_INT_GET_SET;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("failed! unable to push json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::push_action_type_noarg_command(int rpc_cmd,
                                                    const char *rpc_name,
                                                    char *ret_string) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  pCmdObj->command = rpc_cmd;
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_ACTION_NO_ARG;
  pCmdObj->action = RPC_SRV_ACT_WRITE;
  if (ret_string != NULL)
    strcpy(pCmdObj->first_arg_param_name, ret_string);
  else
    pCmdObj->first_arg_param_name[0] = '\0';
  strcpy(pCmdObj->set_rpc_name, rpc_name);
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_action_type_noarg_command: unable to push json-req-task-obj "
           "to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::push_single_int_get_set_command(
    int getcmd, int setcmd, const char *get_rpc_name, const char *set_rpc_name,
    char *param_name, char *subarg, int readonly) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  strcpy(pCmdObj->get_rpc_name, get_rpc_name);
  strcpy(pCmdObj->set_rpc_name, set_rpc_name);
  if (readonly) {
    pCmdObj->command = getcmd;
    pCmdObj->action = RPC_SRV_ACT_READ;
    strcpy(pCmdObj->first_arg_param_name, param_name);
  } else if (get_next_subargument(&subarg) == 0) {
    pCmdObj->command = getcmd;
    pCmdObj->action = RPC_SRV_ACT_READ;
    strcpy(pCmdObj->first_arg_param_name, param_name);
  } else {
    pCmdObj->command = setcmd;
    pCmdObj->cmd_int_val = atoi(subarg);
    pCmdObj->first_arg_param_int_value = atoi(subarg);
    strcpy(pCmdObj->first_arg_param_name, param_name);
    pCmdObj->action = RPC_SRV_ACT_WRITE;
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_INT_GET_SET;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_int_type_get_set_command: failed! unable to push "
           "json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::push_int_get_set_with_dev_addr_arg_command(
    int getcmd, int setcmd, const char *get_rpc_name, const char *set_rpc_name,
    char *int_param_name, char *addr_param_name, int addr_param_val,
    char *subarg, RPC_SRV_ACT forced_act) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  strcpy(pCmdObj->get_rpc_name, get_rpc_name);
  strcpy(pCmdObj->set_rpc_name, set_rpc_name);
  if (addr_param_val >= 0) {
    if (get_next_subargument(&subarg) == 0) {
      if (forced_act == RPC_SRV_ACT_WRITEONLY) {
        printf("this is a write only command!!!\n");
        return -1;
      }
      strcpy(pCmdObj->first_arg_param_name, addr_param_name);
      pCmdObj->first_arg_param_int_value = addr_param_val;
      strcpy(pCmdObj->second_arg_param_name, int_param_name);
      pCmdObj->command = getcmd;
      pCmdObj->action = RPC_SRV_ACT_READ;
    } else {
      if (forced_act == RPC_SRV_ACT_READONLY) {
        printf("this is a read only command!!!\n");
        return -1;
      }
      strcpy(pCmdObj->first_arg_param_name, addr_param_name);
      pCmdObj->first_arg_param_int_value = addr_param_val;
      strcpy(pCmdObj->second_arg_param_name, int_param_name);
      pCmdObj->second_arg_param_int_value = atoi(subarg);
      pCmdObj->command = setcmd;
      pCmdObj->action = RPC_SRV_ACT_WRITE;
    }
  } else {
    if (get_next_subargument(&subarg) == 0) {
      printf("please specify address!!!\n");
      return -1;
    }
    strcpy(pCmdObj->first_arg_param_name, addr_param_name);
    pCmdObj->first_arg_param_int_value = atoi(subarg);
    strcpy(pCmdObj->second_arg_param_name, int_param_name);
    if (get_next_subargument(&subarg) == 0) {
      pCmdObj->command = getcmd;
      pCmdObj->action = RPC_SRV_ACT_READ;
    } else {
      pCmdObj->second_arg_param_int_value = atoi(subarg);
      pCmdObj->command = setcmd;
      pCmdObj->action = RPC_SRV_ACT_WRITE;
    }
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_INT_GET_SET_WITH_DEV_ADDR;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_int_type_get_set_command: failed! unable to push "
           "json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::push_enum_get_set_with_dev_addr_arg_command(
    int getcmd, int setcmd, const char *get_rpc_name, const char *set_rpc_name,
    const char **enum_string_table, int enum_max_val, char *enum_param_name,
    char *addr_param_name, int addr_param_val, char *subarg,
    RPC_SRV_ACT forced_act) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  strcpy(pCmdObj->get_rpc_name, get_rpc_name);
  strcpy(pCmdObj->set_rpc_name, set_rpc_name);
  if (get_next_subargument(&subarg) == 0) {
    if (forced_act == RPC_SRV_ACT_WRITEONLY) {
      printf("this is a write only command!!!\n");
      return -1;
    }
    strcpy(pCmdObj->first_arg_param_name, addr_param_name);
    pCmdObj->first_arg_param_int_value = addr_param_val;
    strcpy(pCmdObj->second_arg_param_name, enum_param_name);
    pCmdObj->command = getcmd;
    pCmdObj->action = RPC_SRV_ACT_READ;
  } else {
    if (forced_act == RPC_SRV_ACT_READONLY) {
      printf("this is a read only command!!!\n");
      return -1;
    }
    strcpy(pCmdObj->first_arg_param_name, addr_param_name);
    pCmdObj->first_arg_param_int_value = addr_param_val;
    pCmdObj->cmd_enum_val =
        string_to_enum(enum_string_table, subarg, enum_max_val);
    if (pCmdObj->cmd_enum_val >= enum_max_val) {
      pCmdObj->command = getcmd;
      pCmdObj->action = RPC_SRV_ACT_READ;
      strcpy(pCmdObj->second_arg_param_name, enum_param_name);
    } else {
      pCmdObj->command = setcmd;
      pCmdObj->action = RPC_SRV_ACT_WRITE;
      strcpy(pCmdObj->second_arg_param_name, enum_param_name);
      strcpy(pCmdObj->second_arg_param_value,
             enum_string_table[pCmdObj->cmd_enum_val]);
    }
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_ENUM_GET_SET_WITH_DEV_ADDR;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_enum_type_get_set_command: failed! unable to push "
           "json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::push_string_get_set_with_dev_addr_arg_command(
    int getcmd, int setcmd, const char *get_rpc_name, const char *set_rpc_name,
    char *string_param_name, char *addr_param_name, int addr_param_val,
    char *subarg, RPC_SRV_ACT forced_act) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  strcpy(pCmdObj->get_rpc_name, get_rpc_name);
  strcpy(pCmdObj->set_rpc_name, set_rpc_name);
  if (get_next_subargument(&subarg) == 0) {
    if (forced_act == RPC_SRV_ACT_WRITEONLY) {
      printf("this is a write only command!!!\n");
      return -1;
    }
    strcpy(pCmdObj->first_arg_param_name, addr_param_name);
    pCmdObj->first_arg_param_int_value = addr_param_val;
    strcpy(pCmdObj->second_arg_param_name, string_param_name);
    pCmdObj->command = getcmd;
    pCmdObj->action = RPC_SRV_ACT_READ;
  } else {
    if (forced_act == RPC_SRV_ACT_READONLY) {
      printf("this is a read only command!!!\n");
      return -1;
    }
    strcpy(pCmdObj->first_arg_param_name, addr_param_name);
    pCmdObj->first_arg_param_int_value = addr_param_val;
    strcpy(pCmdObj->second_arg_param_name, string_param_name);
    strcpy(pCmdObj->second_arg_param_value, subarg);
    pCmdObj->command = setcmd;
    pCmdObj->action = RPC_SRV_ACT_WRITE;
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_STRING_GET_SET_WITH_DEV_ADDR;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_string_type_get_set_command: failed! unable to push "
           "json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::push_single_enum_get_set_command(
    int getcmd, int setcmd, const char *get_rpc_name, const char *set_rpc_name,
    const char **enum_string_table, int enum_max_val, char *param_name,
    char *subarg, char *ret_string) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  strcpy(pCmdObj->get_rpc_name, get_rpc_name);
  strcpy(pCmdObj->set_rpc_name, set_rpc_name);
  if (get_next_subargument(&subarg) == 0) {
    pCmdObj->command = getcmd;
    pCmdObj->action = RPC_SRV_ACT_READ;
    strcpy(pCmdObj->first_arg_param_name, param_name);
  } else {
    pCmdObj->cmd_enum_val =
        string_to_enum(enum_string_table, subarg, enum_max_val);
    if (pCmdObj->cmd_enum_val >= enum_max_val) {
      pCmdObj->command = getcmd;
      pCmdObj->action = RPC_SRV_ACT_READ;
      strcpy(pCmdObj->first_arg_param_name, param_name);
    } else {
      pCmdObj->command = setcmd;
      pCmdObj->action = RPC_SRV_ACT_WRITE;
      strcpy(pCmdObj->first_arg_param_name, param_name);
      strcpy(pCmdObj->first_arg_param_value,
             enum_string_table[pCmdObj->cmd_enum_val]);
      if (ret_string != NULL)
        strcpy(pCmdObj->second_arg_param_name, ret_string);
      else
        pCmdObj->second_arg_param_name[0] = '\0';
    }
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_ENUM_GET_SET;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_single_enum_get_set_command: failed! unable to push "
           "json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
#include <sys/stat.h>
int ADCmdlineHelper::Is_it_valid_file(char *filepath) {
  struct stat buffer;
  if (stat(filepath, &buffer) != 0)
    return -1;
  if (buffer.st_mode & S_IFREG)
    return 0;
  return -1;
}
int ADCmdlineHelper::parse_ip_list_opt(char *subarg) {
  bool FileChecked = false;
  while (get_next_subargument(&subarg) != 0) {
    if (FileChecked == false) {
      if (Is_it_valid_file(subarg) != 0)
        FileChecked = true;
      else {
        push_ip_to_list_from_file(subarg);
        break;
      }
    }
    strcpy(ip, subarg);
    push_ip_to_list(subarg);
  }
  return 0;
}
int ADCmdlineHelper::parse_port_number_opt(char *subarg) {
  if (get_next_subargument(&subarg) != 0)
    port = atoi(subarg);
  return 0;
}
int ADCmdlineHelper::parse_emulation_mode_opt(char *subarg) {
  if (get_next_subargument(&subarg) != 0) {
    if (strcmp(subarg, "yes") == 0)
      emulation_mode = CMDLINE_OPT_TYPE_YES;
  }
  return 0;
}
int ADCmdlineHelper::parse_socket_log_opt(char *subarg) {
  if (get_next_subargument(&subarg) != 0) {
    if (strcmp(subarg, "yes") == 0)
      socket_log = CMDLINE_OPT_TYPE_YES;
  }
  return 0;
}
int ADCmdlineHelper::get_socket_log_opt() {
  if (socket_log == CMDLINE_OPT_TYPE_YES)
    return 1;
  else
    return 0;
}
bool ADCmdlineHelper::get_debug_log_opt() {
  if (debug_log == CMDLINE_OPT_TYPE_YES)
    return true;
  else
    return false;
}
int ADCmdlineHelper::parse_board_type_opt(char *subarg) {
  if (get_next_subargument(&subarg) == 0)
    return 0;
  const char *table[] = ADCMN_BOARD_TYPE_TABLE;
  int result;
  ADCmnStringProcessor string_proc;
  result = string_proc.string_to_enum(table, subarg, ADCMN_BOARD_TYPE_UNKNOWN);
  if (result >= ADCMN_BOARD_TYPE_UNKNOWN)
    DeviceInfo.BoardType = ADCMN_BOARD_TYPE_UNKNOWN;
  else
    DeviceInfo.BoardType = (ADCMN_BOARD_TYPE)result;
  return 0;
}
int ADCmdlineHelper::get_dev_info(ADCMN_DEV_INFO *pInfo) {
  pInfo->BoardType = DeviceInfo.BoardType;
  return 0;
}
int ADCmdlineHelper::grep_ip_line(char *ip_filepath, int line, char *ip) {
  char command[1024];
  char temp_str[256];
  sprintf(command, "sed '%d,%d!d' %s", line, line, ip_filepath);
  FILE *shell;
  shell = popen(command, "r");
  if (shell == NULL)
    return -1;
  if (fgets(temp_str, 254, shell) == NULL) {
    pclose(shell);
    return -1;
  }
  if (temp_str[strlen(temp_str) - 1] == '\n')
    temp_str[strlen(temp_str) - 1] = '\0';
  temp_str[255] = '\0';
  strcpy(ip, temp_str);
  pclose(shell);
  return 0;
}
int ADCmdlineHelper::count_total_ip_from_file(char *ip_filepath) {
  int count = 0;
  char command[1024];
  char temp_str[256];
  sprintf(command, "sed -n '$=' %s", ip_filepath);
  FILE *shell;
  shell = popen(command, "r");
  if (shell == NULL)
    return -1;
  if (fgets(temp_str, 250, shell) == NULL) {
    pclose(shell);
    return -1;
  }
  temp_str[255] = '\0';
  count = atoi(temp_str);
  pclose(shell);
  return count;
}
int ADCmdlineHelper::push_ip_to_list_from_file(char *file) {
  char ip[256];
  int total_ip = count_total_ip_from_file(file);
  for (int i = 0; i < total_ip; i++) {
    if (grep_ip_line(file, i + 1, ip) == 0)
      push_ip_to_list(ip);
  }
  return 0;
}
int ADCmdlineHelper::push_ip_to_list(char *ip) {
  IpAddrEntryObj *pIPEntry = NULL;
  OBJECT_MEM_NEW(pIPEntry, IpAddrEntryObj);
  if (pIPEntry == NULL) {
    printf("failed! unable to allocate memory for ipEntry!\n");
    return -1;
  }
  strcpy(pIPEntry->ip_addr, ip);
  if (ipChain.chain_put((void *)pIPEntry) != 0) {
    printf("failed! unable to push ip entry to chain!\n");
    OBJ_MEM_DELETE(pIPEntry);
    return -1;
  }
  return 0;
}
#define MAX_SRV_RESPONSE_TIMEOUT 120000
int ADCmdlineHelper::wait_for_client_workers_to_finish() {
  ADThreadedSockClient *pWorker = NULL;
  int commands = ClientWorkersList.get_chain_size();
  for (int i = 0; i < commands; i++) {
    pWorker = NULL;
    pWorker = (ADThreadedSockClient *)ClientWorkersList.chain_get_by_index(i);
    if (pWorker != NULL) {
      int SrvRespTimeout = 0;
      while (pWorker->is_running() &&
             (SrvRespTimeout++ < MAX_SRV_RESPONSE_TIMEOUT)) {
        usleep(5000);
      }
    }
    pWorker->print_command_result();
  }
  return 0;
}
int ADCmdlineHelper::delete_client_workers() {
  ADThreadedSockClient *pWorker = NULL;
  while ((pWorker = (ADThreadedSockClient *)ClientWorkersList.chain_get()) !=
         NULL) {
    OBJ_MEM_DELETE(pWorker);
  }
  return 0;
}
int ADCmdlineHelper::run_user_command(CmdExecutionObj *pCmdObj,
                                      ADJsonRpcClient *pSrvSockConn,
                                      ADGenericChain *pOutMsgList,
                                      ADThreadedSockClientProducer *pWorker) {
  return run_subscribers_commands(pCmdObj, pSrvSockConn, pOutMsgList, pWorker);
}
int ADCmdlineHelper::start_new_client_worker(char *ip_addr, int port) {
  ADThreadedSockClient *pWorker = NULL;
  OBJECT_MEM_NEW(pWorker, ADThreadedSockClient);
  if (pWorker == NULL) {
    printf("failed! unable to allocate memory for pWorker!\n");
    return -1;
  }
  pWorker->attach_helper(this);
  pWorker->run_commands(ip_addr, port, &CmdChain);
  if (ClientWorkersList.chain_put((void *)pWorker) != 0) {
    printf("failed! unable to push pWorker entry to chain!\n");
    OBJ_MEM_DELETE(pWorker);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::run_commands() {
  IpAddrEntryObj *pIpObj = NULL;
  int commands = CmdChain.get_chain_size();
  if (commands == 0) {
    printf("no command execution is requested!!! try with --help\n");
    return -1;
  }
  if (ipChain.get_chain_size() == 0)
    push_ip_to_list(ip);
  while ((pIpObj = (IpAddrEntryObj *)ipChain.chain_get()) != NULL) {
    start_new_client_worker(pIpObj->ip_addr, port);
    OBJ_MEM_DELETE(pIpObj);
  }
  wait_for_client_workers_to_finish();
  delete_client_workers();
  return 0;
}
int ADCmdlineHelper::push_string_get_set_with_string_arg_command(
    int getcmd, int setcmd, const char *get_rpc_name, const char *set_rpc_name,
    char *first_par_name, char *first_par_val, char *sec_para_name,
    char *sec_para_val, char *subarg, RPC_SRV_ACT forced_act) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  strcpy(pCmdObj->get_rpc_name, get_rpc_name);
  strcpy(pCmdObj->set_rpc_name, set_rpc_name);
  strcpy(pCmdObj->first_arg_param_name, first_par_name);
  strcpy(pCmdObj->first_arg_param_value, first_par_val);
  strcpy(pCmdObj->second_arg_param_name, sec_para_name);
  strcpy(pCmdObj->second_arg_param_value, sec_para_val);
  if (get_next_subargument(&subarg) == 0) {
    if (forced_act == RPC_SRV_ACT_WRITEONLY) {
      printf("this is a write only command!!!\n");
      return -1;
    }
    pCmdObj->command = getcmd;
    pCmdObj->action = RPC_SRV_ACT_READ;
  } else {
    if (forced_act == RPC_SRV_ACT_READONLY) {
      printf("this is a read only command!!!\n");
      return -1;
    }
    pCmdObj->command = setcmd;
    pCmdObj->action = RPC_SRV_ACT_WRITE;
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_STRING_GET_SET_WITH_STRING_PARA;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_string_type_get_set_command: failed! unable to push "
           "json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::push_get_task_progress_command_with_dev_addr(
    char *get_rpc_name, char *addr, int addr_val, char *taskID_name,
    char *taskSts_name, int command, char *subarg) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  strcpy(pCmdObj->get_rpc_name, get_rpc_name);
  strcpy(pCmdObj->set_rpc_name, get_rpc_name);
  if (get_next_subargument(&subarg) == 0) {
    OBJ_MEM_DELETE(pCmdObj);
    printf("please specify correct task ID!!!\n");
    return -1;
  } else {
    strcpy(pCmdObj->first_arg_param_name, addr);
    pCmdObj->first_arg_param_int_value = addr_val;
    strcpy(pCmdObj->second_arg_param_name, taskID_name);
    strcpy(pCmdObj->second_arg_param_value, subarg);
    pCmdObj->command = command;
    pCmdObj->action = RPC_SRV_ACT_READ;
    strcpy(pCmdObj->third_arg_param_name, taskSts_name);
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_GET_TASK_STATUS_WITH_DEV_ADDR;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_get_task_progress_command: failed! unable to push "
           "json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::push_get_task_progress_command(char *get_rpc_name,
                                                    char *taskID_name,
                                                    char *taskSts_name,
                                                    int command, char *subarg) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  strcpy(pCmdObj->get_rpc_name, get_rpc_name);
  strcpy(pCmdObj->set_rpc_name, get_rpc_name);
  if (get_next_subargument(&subarg) == 0) {
    OBJ_MEM_DELETE(pCmdObj);
    printf("please specify correct task ID!!!\n");
    return -1;
  } else {
    strcpy(pCmdObj->first_arg_param_name, taskID_name);
    strcpy(pCmdObj->first_arg_param_value, subarg);
    pCmdObj->command = command;
    pCmdObj->action = RPC_SRV_ACT_READ;
    strcpy(pCmdObj->second_arg_param_name, taskSts_name);
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_GET_TASK_STATUS;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_get_task_progress_command: failed! unable to push "
           "json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::push_string_get_set_with_enum_arg(
    int getcmd, int setcmd, const char *get_rpc, const char *set_rpc,
    const char *enum_name, int enum_max, const char **enum_table,
    const char *string_name, char *subarg, RPC_SRV_ACT forced_act) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  strcpy(pCmdObj->get_rpc_name, get_rpc);
  strcpy(pCmdObj->set_rpc_name, set_rpc);
  if (get_next_subargument(&subarg) == 0) {
    OBJ_MEM_DELETE(pCmdObj);
    printf("please specify correct enum value!!!\n");
    return -1;
  } else {
    int temp_val = string_to_enum(enum_table, subarg, enum_max);
    if (temp_val >= enum_max) {
      OBJ_MEM_DELETE(pCmdObj);
      printf("please specify correct first enum type!!\n");
      return -1;
    } else {
      strcpy(pCmdObj->first_arg_param_name, enum_name);
      strcpy(pCmdObj->first_arg_param_value, enum_table[temp_val]);
      strcpy(pCmdObj->second_arg_param_name, string_name);
      if (forced_act == RPC_SRV_ACT_READ_AND_WRITE) {
        if (get_next_subargument(&subarg) == 0) {
          pCmdObj->command = getcmd;
          pCmdObj->action = RPC_SRV_ACT_READ;
        } else {
          pCmdObj->command = setcmd;
          pCmdObj->action = RPC_SRV_ACT_WRITE;
          strcpy(pCmdObj->second_arg_param_name, string_name);
          strcpy(pCmdObj->second_arg_param_value, subarg);
        }
      }
    }
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_STRING_GET_SET_WITH_ENUM_PARA;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_string_get_set_with_enum_arg: failed! unable to push "
           "json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::push_string_get_set_command(
    int getcmd, int setcmd, const char *get_rpc_name, const char *set_rpc_name,
    char *string_param_name, char *subarg, RPC_SRV_ACT forced_act) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  strcpy(pCmdObj->get_rpc_name, get_rpc_name);
  strcpy(pCmdObj->set_rpc_name, set_rpc_name);
  if (get_next_subargument(&subarg) == 0) {
    if (forced_act == RPC_SRV_ACT_WRITEONLY) {
      printf("this is a write only command!!!\n");
      return -1;
    }
    strcpy(pCmdObj->first_arg_param_name, string_param_name);
    pCmdObj->command = getcmd;
    pCmdObj->action = RPC_SRV_ACT_READ;
  } else {
    if (forced_act == RPC_SRV_ACT_READONLY) {
      printf("this is a read only command!!!\n");
      return -1;
    }
    strcpy(pCmdObj->first_arg_param_name, string_param_name);
    strcpy(pCmdObj->first_arg_param_value, subarg);
    pCmdObj->command = setcmd;
    pCmdObj->action = RPC_SRV_ACT_WRITE;
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_STRING_GET_SET;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_string_type_get_set_command: failed! unable to push "
           "json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
int ADCmdlineHelper::push_double_int_set_command(int setcmd,
                                                 const char *set_rpc_name,
                                                 char *param1_name,
                                                 char *param2_name,
                                                 char *subarg) {
  CmdExecutionObj *pCmdObj = NULL;
  OBJECT_MEM_NEW(pCmdObj, CmdExecutionObj);
  if (pCmdObj == NULL) {
    printf("failed create pCmdObj!!!\n");
    return -1;
  }
  strcpy(pCmdObj->set_rpc_name, set_rpc_name);
  if (get_next_subargument(&subarg) == 0) {
    OBJ_MEM_DELETE(pCmdObj);
    printf("please specify first int argument!!!\n");
    return -1;
  } else {
    pCmdObj->command = setcmd;
    pCmdObj->cmd_int_val = atoi(subarg);
    pCmdObj->first_arg_param_int_value = atoi(subarg);
    strcpy(pCmdObj->first_arg_param_name, param1_name);
    pCmdObj->action = RPC_SRV_ACT_WRITE;
    if (get_next_subargument(&subarg) == 0) {
      OBJ_MEM_DELETE(pCmdObj);
      printf("please specify second int argument!!!\n");
      return -1;
    } else {
      pCmdObj->second_arg_param_int_value = atoi(subarg);
      strcpy(pCmdObj->second_arg_param_name, param2_name);
    }
  }
  pCmdObj->cmd_type = CLIENT_CMD_TYPE_DOUBLE_INT_SET;
  if (CmdChain.chain_put((void *)pCmdObj) != 0) {
    printf("push_double_int_set_command: failed! unable to push "
           "json-req-task-obj to chain!\n");
    OBJ_MEM_DELETE(pCmdObj);
    return -1;
  }
  return 0;
}
