#include "ADTaskWorker.hpp"
#include "ADJsonRpcClient.hpp"
#include <stdio.h>
int ADTaskWorkerProducer::IDGenerator = 0;
int ADTaskWorker::identify_chain_element(void *element, int ident,
                                         ADChainProducer *pObj) {
  int call_from = pObj->getID();
  if (call_from == work_inprog_chain_id) {
    WORK_CMD_TASK_IN_PROG *pPtr;
    pPtr = (WORK_CMD_TASK_IN_PROG *)element;
    if (pPtr->taskID == ident)
      return 0;
    else
      return -1;
  } else if (call_from == work_chain_id) {
    WORK_CMD_TASK *pPtr;
    pPtr = (WORK_CMD_TASK *)element;
    if (pPtr->taskID == ident)
      return 0;
    else
      return -1;
  } else
    return -1;
}
int ADTaskWorker::free_chain_element_data(void *element,
                                          ADChainProducer *pObj) {
  int call_from = pObj->getID();
  if (call_from == work_chain_id) {
    WORK_CMD_TASK *pPtr;
    pPtr = (WORK_CMD_TASK *)element;
    OBJ_MEM_DELETE(pPtr->pWorkData);
  }
  return 0;
}
int ADTaskWorker::monoshot_callback_function(void *pUserData,
                                             ADThreadProducer *pObj) {
  WORK_CMD_TASK *work_obj = NULL;
  work_obj = (WORK_CMD_TASK *)work_chain.chain_get();
  if (work_obj != NULL) {
    RPC_SRV_RESULT task_result =
        run_work(work_obj->command, work_obj->pWorkData);
    WORK_CMD_TASK_IN_PROG *work_inprog_obj = NULL;
    if (work_obj->resp_type == ADLIB_ASYNC_RESP_TYPE_TRIGGER)
      work_obj->done_action = WORK_CMD_AFTER_DONE_DELETE;
    if (work_obj->done_action == WORK_CMD_AFTER_DONE_PRESERVE) {
      work_inprog_chain.chain_lock();
      work_inprog_obj =
          (WORK_CMD_TASK_IN_PROG *)work_inprog_chain.chain_get_by_ident(
              work_obj->taskID);
      if (work_inprog_obj != NULL) {
        work_inprog_obj->taskSts = task_result;
        work_inprog_obj->percent_complete = 100;
      }
      work_inprog_chain.chain_unlock();
      NOTIFY_EVENT(ADLIB_EVENT_NUM_INPROG_DONE, work_obj->taskID, notifyPortNum,
                   task_result);
    } else {
      work_inprog_obj =
          (WORK_CMD_TASK_IN_PROG *)work_inprog_chain.chain_remove_by_ident(
              work_obj->taskID);
      if (work_inprog_obj != NULL) {
        OBJ_MEM_DELETE(work_inprog_obj);
      }
    }
    OBJ_MEM_DELETE(work_obj);
  }
  return 0;
}
ADTaskWorker::ADTaskWorker() {
  notifyPortNum = -1;
  work_inprog_chain_id = work_inprog_chain.attach_helper(this);
  work_chain_id = work_chain.attach_helper(this);
  work_thread.subscribe_thread_callback(this);
  work_thread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  work_thread.start_thread();
}
ADTaskWorker::~ADTaskWorker() {
  work_thread.stop_thread();
  work_chain.remove_all();
  work_inprog_chain.remove_all();
}
RPC_SRV_RESULT ADTaskWorker::get_task_status(int taskID, int *taskSts,
                                             char *errMsg) {
  RPC_SRV_RESULT ret_val;
  WORK_CMD_TASK_IN_PROG *work_inprog_obj = NULL;
  work_inprog_chain.chain_lock();
  work_inprog_obj =
      (WORK_CMD_TASK_IN_PROG *)work_inprog_chain.chain_get_by_ident(taskID);
  if (work_inprog_obj == NULL) {
    work_inprog_chain.chain_unlock();
    *taskSts = (int)RPC_SRV_RESULT_TASK_ID_NOT_FOUND;
    return RPC_SRV_RESULT_SUCCESS;
  }
  if (work_inprog_obj->taskSts == RPC_SRV_RESULT_IN_PROG) {
    work_inprog_chain.chain_unlock();
    *taskSts = (int)RPC_SRV_RESULT_IN_PROG;
    return RPC_SRV_RESULT_SUCCESS;
  }
  work_inprog_chain.chain_unlock();
  work_inprog_obj =
      (WORK_CMD_TASK_IN_PROG *)work_inprog_chain.chain_remove_by_ident(taskID);
  if (work_inprog_obj == NULL) {
    *taskSts = (int)RPC_SRV_RESULT_MEM_ERROR;
    return RPC_SRV_RESULT_SUCCESS;
  }
  if (work_inprog_obj->taskSts == RPC_SRV_RESULT_SUCCESS) {
    *taskSts = (int)RPC_SRV_RESULT_SUCCESS;
    ret_val = RPC_SRV_RESULT_SUCCESS;
  } else {
    *taskSts = (int)work_inprog_obj->taskSts;
    ret_val = RPC_SRV_RESULT_SUCCESS;
  }
  OBJ_MEM_DELETE(work_inprog_obj);
  return ret_val;
}
int ADTaskWorker::is_command_in_progress(int cmd) { return 0; }
int ADTaskWorker::push_task(int work_cmd, unsigned char *pWorkData, int *taskID,
                            WORK_CMD_AFTER_DONE done) {
  WORK_CMD_TASK *work_obj = NULL;
  WORK_CMD_TASK_IN_PROG *work_inprog_obj = NULL;
  OBJECT_MEM_NEW(work_obj, WORK_CMD_TASK);
  if (work_obj == NULL) {
    return -1;
  }
  OBJECT_MEM_NEW(work_inprog_obj, WORK_CMD_TASK_IN_PROG);
  if (work_inprog_obj == NULL) {
    OBJ_MEM_DELETE(work_obj);
    return -1;
  }
  *taskID = work_inprog_chain.chain_generate_ident();
  work_obj->taskID = *taskID;
  work_obj->percent_complete = 0;
  work_obj->command = work_cmd;
  work_obj->taskSts = RPC_SRV_RESULT_IN_PROG;
  work_obj->done_action = done;
  work_obj->pWorkData = pWorkData;
  work_inprog_obj->taskID = *taskID;
  work_inprog_obj->percent_complete = 0;
  work_inprog_obj->command = work_cmd;
  work_inprog_obj->taskSts = RPC_SRV_RESULT_IN_PROG;
  work_inprog_obj->task_err_message[0] = '\0';
  if (work_chain.chain_put((void *)work_obj) != 0) {
    OBJ_MEM_DELETE(work_obj);
    OBJ_MEM_DELETE(work_inprog_obj);
    printf("unable to put item in work_chain chain\n");
    return -1;
  }
  if (work_inprog_chain.chain_put((void *)work_inprog_obj) != 0) {
    work_obj = (WORK_CMD_TASK *)work_chain.chain_remove_by_ident(*taskID);
    if (work_obj != NULL)
      OBJ_MEM_DELETE(work_obj);
    OBJ_MEM_DELETE(work_inprog_obj);
    printf("unable to put item in work_in_prog chain\n");
    return -1;
  }
  work_thread.wakeup_thread();
  return 0;
}
RPC_SRV_RESULT ADTaskWorker::reset_task_id_and_chain() {
  work_inprog_chain.chain_empty();
  return RPC_SRV_RESULT_SUCCESS;
}
