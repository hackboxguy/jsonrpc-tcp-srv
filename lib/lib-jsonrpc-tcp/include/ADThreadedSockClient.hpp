#ifndef __AD_THREADED_SOCK_CLIENT_H_
#define __AD_THREADED_SOCK_CLIENT_H_
#include "ADCmnCltCmdProcessor.hpp"
#include "ADGenericChain.hpp"
#include "ADJsonRpcClient.hpp"
#include "ADThread.hpp"
class ADThreadedSockClientProducer;
class ADThreadedSockClientConsumer {
public:
  virtual int run_user_command(CmdExecutionObj *pCmdObj,
                               ADJsonRpcClient *pSrvSockConn,
                               ADGenericChain *pOutMsgList,
                               ADThreadedSockClientProducer *pWorker) = 0;
  virtual ~ADThreadedSockClientConsumer(){};
};
class ADThreadedSockClientProducer {
  static int IDGenerator;
  ADThreadedSockClientConsumer *pConsumer;
  int id;

protected:
  int is_helper_attached(void) {
    if (pConsumer == NULL)
      return -1;
    return 0;
  }
  int run_user_command(CmdExecutionObj *pCmdObj, ADJsonRpcClient *pSrvSockConn,
                       ADGenericChain *pOutMsgList) {
    if (pConsumer != NULL)
      return pConsumer->run_user_command(pCmdObj, pSrvSockConn, pOutMsgList,
                                         this);
    return -1;
  }

public:
  ADThreadedSockClientProducer() {
    id = IDGenerator++;
    pConsumer = NULL;
  }
  virtual ~ADThreadedSockClientProducer(){};
  int attach_helper(ADThreadedSockClientConsumer *c) {
    if (pConsumer == NULL) {
      pConsumer = c;
      return id;
    } else
      return -1;
  }
  int getID() { return id; }
};
class ADThreadedSockClient : public ADThreadedSockClientProducer,
                             public ADChainConsumer,
                             public ADThreadConsumer,
                             public ADCmnCltCmdProcessor {
  char ip_addr[255];
  int port;
  unsigned char running;
  unsigned char stop_running;
  ADThread cmd_thread;
  ADGenericChain cmd_chain;
  ADGenericChain output_msg_chain;
  int output_msg_chain_id;
  virtual int identify_chain_element(void *element, int ident,
                                     ADChainProducer *pObj);
  virtual int double_identify_chain_element(void *element, int ident1,
                                            int ident2, ADChainProducer *pObj);
  virtual int free_chain_element_data(void *element, ADChainProducer *pObj);
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj);
  virtual int thread_callback_function(void *pUserData, ADThreadProducer *pObj);
  int initialize_helpers(void);
  int start_command_execution();
  int stop_command_execution();
  int copy_command_chain(ADGenericChain *pDest, ADGenericChain *pSrc);
  int print_output_message(OutputPrintMsgObj *pOutput);

public:
  ADThreadedSockClient();
  ~ADThreadedSockClient();
  int run_commands(char *srv_ip, int srv_port, ADGenericChain *pSrcChain);
  int is_running();
  int print_command_result();
  int my_log_print_message(ADJsonRpcClient *pSrvSockConn, char *rpc_method_name,
                           RPC_SRV_ACT action, char *result,
                           ADGenericChain *pOutMsgList);
};
#endif
