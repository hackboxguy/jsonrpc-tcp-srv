#ifndef __AD_TASK_WORKER_H_
#define __AD_TASK_WORKER_H_
#include "ADCommon.hpp"
#include "ADGenericChain.hpp"
#include "ADThread.hpp"
typedef enum WORK_CMD_AFTER_DONE_T {
  WORK_CMD_AFTER_DONE_PRESERVE,
  WORK_CMD_AFTER_DONE_DELETE,
  WORK_CMD_AFTER_DONE_UNKNOWN,
  WORK_CMD_AFTER_DONE_NONE
} WORK_CMD_AFTER_DONE;
typedef struct WORK_CMD_TASK_T {
  int taskID;
  int percent_complete;
  int command;
  RPC_SRV_RESULT taskSts;
  WORK_CMD_AFTER_DONE done_action;
  ADLIB_ASYNC_RESP_TYPE resp_type;
  unsigned char *pWorkData;

public:
  WORK_CMD_TASK_T() { resp_type = ADLIB_ASYNC_RESP_TYPE_POLL; }
} WORK_CMD_TASK;
typedef struct WORK_CMD_TASK_IN_PROG_T {
  int taskID;
  int percent_complete;
  int command;
  RPC_SRV_RESULT taskSts;
  char task_err_message[255];
} WORK_CMD_TASK_IN_PROG;
class ADTaskWorkerProducer;
class ADTaskWorkerConsumer {
public:
  virtual RPC_SRV_RESULT run_work(int cmd, unsigned char *pWorkData,
                                  ADTaskWorkerProducer *pTaskWorker) = 0;
  virtual ~ADTaskWorkerConsumer(){};
};
class ADTaskWorkerProducer {
  static int IDGenerator;
  ADTaskWorkerConsumer *pConsumer;
  int id;

protected:
  RPC_SRV_RESULT run_work(int cmd, unsigned char *pWorkData) {
    if (pConsumer != NULL) {
      return pConsumer->run_work(cmd, pWorkData, this);
    }
    return RPC_SRV_RESULT_UNKNOWN;
  }

public:
  ADTaskWorkerProducer() {
    id = IDGenerator++;
    pConsumer = NULL;
  }
  virtual ~ADTaskWorkerProducer(){};
  int attach_helper(ADTaskWorkerConsumer *c) {
    if (pConsumer == NULL) {
      pConsumer = c;
      return id;
    } else {
      return -1;
    }
  }
  int getID() { return id; }
};
class ADTaskWorker : public ADTaskWorkerProducer,
                     public ADChainConsumer,
                     public ADThreadConsumer {
  ADThread work_thread;
  ADGenericChain work_chain;
  ADGenericChain work_inprog_chain;
  int work_chain_id;
  int work_inprog_chain_id;
  virtual int identify_chain_element(void *element, int ident,
                                     ADChainProducer *pObj);
  virtual int double_identify_chain_element(void *element, int ident1,
                                            int ident2, ADChainProducer *pObj) {
    return 0;
  };
  virtual int free_chain_element_data(void *element, ADChainProducer *pObj);
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj);
  virtual int thread_callback_function(void *pUserData,
                                       ADThreadProducer *pObj) {
    return 0;
  };

public:
  ADTaskWorker();
  ~ADTaskWorker();
  int notifyPortNum;
  RPC_SRV_RESULT get_task_status(int taskID, int *taskSts, char *errMsg);
  int is_command_in_progress(int cmd);
  int push_task(int work_cmd, unsigned char *pWorkData, int *taskID,
                WORK_CMD_AFTER_DONE done = WORK_CMD_AFTER_DONE_PRESERVE);
  RPC_SRV_RESULT reset_task_id_and_chain();
};
#endif
