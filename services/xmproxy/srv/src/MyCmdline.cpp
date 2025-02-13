#include "MyCmdline.h"
using namespace std;
/*****************************************************************************/
typedef enum XMPROXY_CMDLINE_OPT_T {
  XMPROXY_CMDLINE_OPT_LOGIN_FILE = 100,
  XMPROXY_CMDLINE_OPT_USBGSM_STS,
  XMPROXY_CMDLINE_OPT_ALIAS_LIST_FILE,
  XMPROXY_CMDLINE_OPT_BOT_NAME_FILE,
  XMPROXY_CMDLINE_OPT_EVNT_SUBSCR_LIST_FILE,
  XMPROXY_CMDLINE_OPT_NETINTERFACE, // iface
  XMPROXY_CMDLINE_OPT_UPDATEURL,    // updateurl
  XMPROXY_CMDLINE_OPT_AIAGENT,      // aiagent
  XMPROXY_CMDLINE_OPT_AIMODEL,      // aimodel
  XMPROXY_CMDLINE_OPT_SYSCFG,       // system-type
  XMPROXY_CMDLINE_OPT_UNKNOWN,
  XMPROXY_CMDLINE_OPT_NONE
} XMPROXY_CMDLINE_OPT;
#define XMPROXY_DEFAULT_LOGIN_FILE_PATH "/mnt/settings/etc/xmpp.login"
/*****************************************************************************/
MyCmdline::MyCmdline(CMDLINE_HELPER_MODE cmdline_mode, int portnum,
                     char *version_str)
    : CmdlineHelper(cmdline_mode) {
  NetInterface = "";
  AiAgentUrl = "";
  AiModel = "";
  SystemConfig = "";
  port_number = portnum;
  strcpy(version_number, version_str);
  CmdlineHelper.attach_helper(this);
  // note:"hviptdln" are already used by the producer class in library
  CmdlineHelper.insert_options_entry((char *)"loginfile", optional_argument,
                                     XMPROXY_CMDLINE_OPT_LOGIN_FILE);
  CmdlineHelper.insert_help_entry(
      (char *)"--loginfile=filepath       (path to the "
              "file having xmpp user/pw details)");
  CmdlineHelper.insert_options_entry((char *)"usbgsm", optional_argument,
                                     XMPROXY_CMDLINE_OPT_USBGSM_STS);
  CmdlineHelper.insert_help_entry((char *)"--usbgsm=sts               (sts=0 "
                                          "means no usbgsm module connected)");
  CmdlineHelper.insert_options_entry((char *)"aliaslist", optional_argument,
                                     XMPROXY_CMDLINE_OPT_ALIAS_LIST_FILE);
  CmdlineHelper.insert_help_entry(
      (char *)"--aliaslist=filepath       (file path of alias list)");
  CmdlineHelper.insert_options_entry((char *)"botname", optional_argument,
                                     XMPROXY_CMDLINE_OPT_BOT_NAME_FILE);
  CmdlineHelper.insert_help_entry(
      (char *)"--botname=filepath         (file path of Bot Name)");
  CmdlineHelper.insert_options_entry((char *)"evntsubscr", optional_argument,
                                     XMPROXY_CMDLINE_OPT_EVNT_SUBSCR_LIST_FILE);
  CmdlineHelper.insert_help_entry((char *)"--evntsubscr=filepath      (file "
                                          "path of event subscriber's list)");
  CmdlineHelper.insert_options_entry((char *)"iface", optional_argument,
                                     XMPROXY_CMDLINE_OPT_NETINTERFACE);
  CmdlineHelper.insert_help_entry((char *)"--iface=ethx               (ip of "
                                          "netinterface to print on display)");
  CmdlineHelper.insert_options_entry((char *)"updateurl", optional_argument,
                                     XMPROXY_CMDLINE_OPT_UPDATEURL);
  CmdlineHelper.insert_help_entry(
      (char *)"--updateurl=filepath       (sysupdate fmw-file url)");

  CmdlineHelper.insert_options_entry((char *)"aiagent", optional_argument,
                                     XMPROXY_CMDLINE_OPT_AIAGENT);
  CmdlineHelper.insert_help_entry(
      (char *)"--aiagent=url              (ollama AI agent url)");
  CmdlineHelper.insert_help_entry(
      (char *)"--aimodel=model            (model to connect)");
  CmdlineHelper.insert_options_entry((char *)"aimodel", optional_argument,
                                     XMPROXY_CMDLINE_OPT_AIMODEL);
  CmdlineHelper.insert_options_entry((char *)"syscfg", optional_argument,
                                     XMPROXY_CMDLINE_OPT_SYSCFG);
  CmdlineHelper.insert_help_entry(
      (char *)"--syscfg=sys_config_str    (specify system type "
              "ex:a5v11-xmpp/a5v11-base/3020f-base/docker)");
  strcpy(LoginFilePath, XMPROXY_DEFAULT_LOGIN_FILE_PATH);
  UsbGSMSts = false;
  AliasListFilePath[0] = '\0';
  BotNameFilePath[0] = '\0';
  EvntSubscrListFilePath[0] = '\0';
  UpdateUrlFilePath[0] = '\0';
}
/*****************************************************************************/
MyCmdline::~MyCmdline() {}
/*****************************************************************************/
// override virtual functions of ADGeneric Chain
int MyCmdline::parse_my_cmdline_options(int arg, char *sub_arg) {
  XMPROXY_CMDLINE_OPT command = (XMPROXY_CMDLINE_OPT)arg;
  switch (command) {
  case XMPROXY_CMDLINE_OPT_LOGIN_FILE:
    if (CmdlineHelper.get_next_subargument(&sub_arg) ==
        0) // no login filepath passed by user
      strcpy(LoginFilePath, XMPROXY_DEFAULT_LOGIN_FILE_PATH);
    else
      strcpy(LoginFilePath, sub_arg);
    break;
  case XMPROXY_CMDLINE_OPT_USBGSM_STS:
    if (CmdlineHelper.get_next_subargument(&sub_arg) ==
        0) // no login filepath passed by user
      UsbGSMSts = false;
    else {
      int sts = atoi(sub_arg);
      if (sts == 1)
        UsbGSMSts = true;
      else
        UsbGSMSts = false;
    }
    break;
  case XMPROXY_CMDLINE_OPT_ALIAS_LIST_FILE:
    if (CmdlineHelper.get_next_subargument(&sub_arg) ==
        0) // no alias-list filepath passed by user
      strcpy(AliasListFilePath, "");
    else
      strcpy(AliasListFilePath, sub_arg);
    break;
  case XMPROXY_CMDLINE_OPT_BOT_NAME_FILE:
    if (CmdlineHelper.get_next_subargument(&sub_arg) ==
        0) // no bot-name filepath passed by user
      strcpy(BotNameFilePath, "");
    else
      strcpy(BotNameFilePath, sub_arg);
    break;
  case XMPROXY_CMDLINE_OPT_EVNT_SUBSCR_LIST_FILE:
    if (CmdlineHelper.get_next_subargument(&sub_arg) ==
        0) // no alias-list filepath passed by user
      strcpy(EvntSubscrListFilePath, "");
    else
      strcpy(EvntSubscrListFilePath, sub_arg);
    break;
  case XMPROXY_CMDLINE_OPT_NETINTERFACE:
    if (CmdlineHelper.get_next_subargument(&sub_arg) ==
        0) // ethx option passed by user
      NetInterface = "";
    else
      NetInterface = sub_arg;
    break;
  case XMPROXY_CMDLINE_OPT_UPDATEURL:
    if (CmdlineHelper.get_next_subargument(&sub_arg) ==
        0) // no updateurl filepath passed by user
      strcpy(UpdateUrlFilePath, "");
    else
      strcpy(UpdateUrlFilePath, sub_arg);
    break;
  case XMPROXY_CMDLINE_OPT_AIAGENT:
    if (CmdlineHelper.get_next_subargument(&sub_arg) ==
        0) // no aiagent-url passed by user
      AiAgentUrl = "";
    else
      AiAgentUrl = sub_arg;
    break;
  case XMPROXY_CMDLINE_OPT_AIMODEL:
    if (CmdlineHelper.get_next_subargument(&sub_arg) ==
        0) // no aimodel passed by user
      AiModel = "";
    else
      AiModel = sub_arg;
    break;
  case XMPROXY_CMDLINE_OPT_SYSCFG:
    if (CmdlineHelper.get_next_subargument(&sub_arg) ==
        0) // no system-config passed by user
      SystemConfig = "none";
    else
      SystemConfig = sub_arg;
    break;
  default:
    return 0;
    break;
  }
  return 0;
}
/*****************************************************************************/
// following function will not be called for server_mode_cmdline_helper
int MyCmdline::run_my_commands(CmdExecutionObj *pCmdObj,
                               ADJsonRpcClient *pSrvSockConn,
                               ADGenericChain *pOutMsgList,
                               ADThreadedSockClientProducer *pWorker) {
  switch (pCmdObj->command) {
  case XMPROXY_CMDLINE_OPT_LOGIN_FILE:
    break;
  default:
    return -1;
    break;
  }
  return 0;
}
/*****************************************************************************/
// following function will not be called for server_mode_cmdline_helper
int MyCmdline::run_my_autotest(char *ip, int interval_us, int max_loop,
                               int test_num) {
  return 0;
}
/*****************************************************************************/
int MyCmdline::print_my_version() {
  printf("version - %s\n", version_number);
  return 0;
}
// default server port number
int MyCmdline::get_my_server_port() {
  return port_number; // SDSMS_JSON_PORT_NUMBER;
}
/*****************************************************************************/
int MyCmdline::parse_cmdline_arguments(int argc, char **argv) {
  return CmdlineHelper.parse_cmdline_arguments(argc, argv);
}
/*****************************************************************************/
bool MyCmdline::get_emulation_mode() {
  if (CmdlineHelper.get_emulation_mode() == CMDLINE_OPT_TYPE_YES)
    return true;
  else
    return false;
}
/*****************************************************************************/
int MyCmdline::is_help_printed() { return CmdlineHelper.is_help_printed(); }
/*****************************************************************************/
int MyCmdline::get_socket_log() { return CmdlineHelper.get_socket_log_opt(); }
bool MyCmdline::get_debug_log() { return CmdlineHelper.get_debug_log_opt(); }
/*****************************************************************************/
int MyCmdline::get_port_number() { return CmdlineHelper.get_port_number(); }
/*****************************************************************************/
int MyCmdline::get_dev_info(ADCMN_DEV_INFO *pInfo) {
  return CmdlineHelper.get_dev_info(pInfo);
}
/*****************************************************************************/
int MyCmdline::get_login_filepath(char *filepath) {
  strcpy(filepath, LoginFilePath);
  return 0;
}
bool MyCmdline::is_usbgsm_connected() { return UsbGSMSts; }
std::string MyCmdline::get_alias_list_filepath() {
  std::string path = AliasListFilePath;
  return path;
}
std::string MyCmdline::get_botname_filepath() {
  std::string path = BotNameFilePath;
  return path;
}
std::string MyCmdline::get_evnt_subscr_list_filepath() {
  std::string path = EvntSubscrListFilePath;
  return path;
}
/*****************************************************************************/
std::string MyCmdline::get_net_interface() { return NetInterface; }
/*****************************************************************************/
std::string MyCmdline::get_updateurl_filepath() {
  std::string updateurl = UpdateUrlFilePath;
  return updateurl;
}
/*****************************************************************************/
std::string MyCmdline::get_ai_agent_url() { return AiAgentUrl; }
/*****************************************************************************/
std::string MyCmdline::get_ai_model() { return AiModel; }
/*****************************************************************************/
std::string MyCmdline::get_sys_config() { return SystemConfig; }
ADCMN_SYSCFG_TYPE MyCmdline::get_sys_config_enum() {
  const char *table[] = ADCMN_SYSCFG_TYPE_TABLE;
  int result;
  ADCmnStringProcessor string_proc;
  ADCMN_SYSCFG_TYPE type = ADCMN_SYSCFG_TYPE_UNKNOWN;
  char *syscfg = (char *)SystemConfig.c_str();
  result = string_proc.string_to_enum(table, syscfg, ADCMN_SYSCFG_TYPE_UNKNOWN);
  if (result >= ADCMN_SYSCFG_TYPE_UNKNOWN)
    type = ADCMN_SYSCFG_TYPE_UNKNOWN;
  else
    type = (ADCMN_SYSCFG_TYPE)result;
  return type;
}
/*****************************************************************************/