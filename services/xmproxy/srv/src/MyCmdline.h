#ifndef __MY_CMDLINE_H_
#define __MY_CMDLINE_H_
#include "ADCmdlineHelper.hpp"
#include "ADCmnStringProcessor.hpp"

class MyCmdline : public ADCmdlineHelperConsumer, public ADCmnStringProcessor {
  int port_number;
  std::string version_number;
  ADCmdlineHelper CmdlineHelper;
  std::string LoginFilePath;
  bool UsbGSMSts;
  std::string AliasListFilePath;
  std::string BotNameFilePath;
  std::string EvntSubscrListFilePath;
  std::string UpdateUrlFilePath;
  std::string NetInterface;
  std::string AiAgentUrl;
  std::string AiModel;
  std::string SystemConfig;
  std::string LogLevel;
  std::string AclFilePath;
  std::string ManifestFilePath;
  std::string SubscrFilePath;
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

public:
  MyCmdline(CMDLINE_HELPER_MODE cmdline_mode, int portnum, char *version_str);
  ~MyCmdline();
  int parse_cmdline_arguments(int argc, char **argv);
  bool get_emulation_mode();
  int is_help_printed();
  int get_socket_log();
  bool get_debug_log();
  int get_port_number();
  int get_dev_info(ADCMN_DEV_INFO *pInfo);
  std::string get_login_filepath();
  bool is_usbgsm_connected();
  std::string get_alias_list_filepath(); // char* filepath);
  std::string get_botname_filepath();
  std::string get_evnt_subscr_list_filepath(); // char* filepath);
  std::string get_net_interface();
  std::string get_updateurl_filepath();
  std::string get_ai_agent_url();
  std::string get_ai_model();
  std::string get_sys_config();
  std::string get_log_level();
  std::string get_acl_filepath();
  std::string get_manifest_filepath();
  std::string get_subscr_filepath();
  ADCMN_SYSCFG_TYPE get_sys_config_enum();
  // service specific part
};
#endif
