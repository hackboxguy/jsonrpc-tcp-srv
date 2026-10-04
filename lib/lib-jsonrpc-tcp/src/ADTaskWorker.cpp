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
    // the real type of the work data is unknown here; by convention it is a
    // trivially destructible packet from OBJECT_MEM_NEW, so release the
    // storage without a typed (sized) delete
    if (pPtr->pWorkData != NULL) {
      ::operator delete(pPtr->pWorkData);
      pPtr->pWorkData = NULL;
    }
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
      if (pEventSink != NULL)
        pEventSink->task_worker_event(ADLIB_EVENT_NUM_INPROG_DONE,
                                      work_obj->taskID, task_result);
      else
        NOTIFY_EVENT(ADLIB_EVENT_NUM_INPROG_DONE, work_obj->taskID,
                     notifyPortNum, task_result);
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
  pEventSink = NULL;
  work_inprog_chain_id = work_inprog_chain.attach_helper(this);
  work_inprog_chain.set_element_deleter(
      &chain_delete_object<WORK_CMD_TASK_IN_PROG>);
  work_chain_id = work_chain.attach_helper(this);
  work_chain.set_element_deleter(&chain_delete_object<WORK_CMD_TASK>);
  work_thread.subscribe_thread_callback(this);
  work_thread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  work_thread.start_thread();
}
ADTaskWorker::~ADTaskWorker() {
  stop();
  work_chain.remove_all();
  work_inprog_chain.remove_all();
}
void ADTaskWorker::stop() { work_thread.stop_thread(); }
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
  // a finished task does not change any more: report the status read under
  // the lock, then drop the record (it may already be gone, H4)
  *taskSts = (int)work_inprog_obj->taskSts;
  ret_val = RPC_SRV_RESULT_SUCCESS;
  work_inprog_chain.chain_unlock();
  work_inprog_obj =
      (WORK_CMD_TASK_IN_PROG *)work_inprog_chain.chain_remove_by_ident(taskID);
  if (work_inprog_obj != NULL)
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
  if (work_inprog_chain.get_chain_size() >= ADTASK_WORKER_MAX_INPROG_TASKS)
    evict_completed_tasks();
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
// drops the oldest finished PRESERVE tasks until the chain is below the cap;
// tasks still in progress are never evicted
void ADTaskWorker::evict_completed_tasks() {
  while (work_inprog_chain.get_chain_size() >= ADTASK_WORKER_MAX_INPROG_TASKS) {
    int victim = -1;
    work_inprog_chain.chain_lock();
    for (int i = 0;; i++) {
      WORK_CMD_TASK_IN_PROG *p =
          (WORK_CMD_TASK_IN_PROG *)work_inprog_chain.chain_get_by_index(i);
      if (p == NULL)
        break;
      if (p->taskSts != RPC_SRV_RESULT_IN_PROG) {
        victim = p->taskID;
        break;
      }
    }
    work_inprog_chain.chain_unlock();
    if (victim < 0)
      return;
    WORK_CMD_TASK_IN_PROG *p =
        (WORK_CMD_TASK_IN_PROG *)work_inprog_chain.chain_remove_by_ident(
            victim);
    if (p != NULL)
      OBJ_MEM_DELETE(p);
  }
}
bool ADTaskWorker::tasks_pending() {
  bool busy = work_chain.get_chain_size() > 0;
  work_inprog_chain.chain_lock();
  for (int i = 0; !busy; i++) {
    WORK_CMD_TASK_IN_PROG *p =
        (WORK_CMD_TASK_IN_PROG *)work_inprog_chain.chain_get_by_index(i);
    if (p == NULL)
      break;
    if (p->taskSts == RPC_SRV_RESULT_IN_PROG)
      busy = true;
  }
  work_inprog_chain.chain_unlock();
  return busy;
}
// Clears the task status records and restarts the task IDs at 1, which
// clients of reset_task_status rely on. Restarting while a task is queued
// or running would hand out IDs that are still in use (finding V2-M3):
// the reset first waits up to ADTASK_WORKER_RESET_WAIT_MS for pending
// tasks (clients send 'trigger_x' and then 'reset_task_status' back to
// back) and is refused with RPC_SRV_RESULT_BUSY only if they still run.
RPC_SRV_RESULT ADTaskWorker::reset_task_id_and_chain() {
  for (int waited = 0; tasks_pending(); waited += 5) {
    if (waited >= ADTASK_WORKER_RESET_WAIT_MS)
      return RPC_SRV_RESULT_BUSY;
    usleep(5000);
  }
  work_inprog_chain.chain_empty();
  return RPC_SRV_RESULT_SUCCESS;
}
