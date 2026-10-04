#ifndef __ADJSON_RPC_H_
#define __ADJSON_RPC_H_
#include "ADCmnDevTypes.h"
#include "ADCmnStringProcessor.hpp"
#include "ADCommon.hpp"
#include "ADEvntMgr.hpp"
#include "ADJsonRpcMapper.hpp"
#include "ADTaskWorker.hpp"
#include "ADTimer.hpp"
#include "JsonCmnDef.h"
#include <iostream>
#include <stdint.h>
#include <string>
#include <typeinfo>
#include <vector>
using namespace std;
typedef enum EJSON_RPCGMGR_CMD_T {
  EJSON_RPCGMGR_GET_TASK_STS = 0,
  EJSON_RPCGMGR_GET_RPC_SRV_VERSION = 1,
  EJSON_RPCGMGR_TRIGGER_DATASAVE = 2,
  EJSON_RPCGMGR_GET_SETTINGS_STS = 3,
  EJSON_RPCGMGR_SHUTDOWN_SERVICE = 4,
  EJSON_RPCGMGR_RESET_TASK_STS = 5,
  EJSON_RPCGMGR_GET_READY_STS = 6,
  EJSON_RPCGMGR_GET_DEBUG_LOG = 7,
  EJSON_RPCGMGR_SET_DEBUG_LOG = 8,
  EJSON_RPCGMGR_TRIGGER_FACTORY_STORE = 9,
  EJSON_RPCGMGR_TRIGGER_FACTORY_RESTORE = 10,
  EJSON_RPCGMGR_EVENT_SUBSCRIBE = 11,
  EJSON_RPCGMGR_EVENT_UNSUBSCRIBE = 12,
  EJSON_RPCGMGR_EVENT_NOTIFY = 13,
  EJSON_RPCGMGR_EVENT_PROCESS = 14,
  EJSON_RPCGMGR_TRIGGER_RUN = 15,
  EJSON_RPCMGR_GET_DEVOP_STATE = 16,
  EJSON_RPCMGR_SET_DEVOP_STATE = 17,
  EJSON_RPCMGR_GET_MW_BYTE = 18,
  EJSON_RPCMGR_SET_MW_BYTE = 19,
  EJSON_RPCMGR_GET_MW_WORD = 20,
  EJSON_RPCMGR_SET_MW_WORD = 21,
  EJSON_RPCMGR_GET_MW_DWORD = 22,
  EJSON_RPCMGR_SET_MW_DWORD = 23,
  EJSON_RPCGMGR_CMD_END,
  EJSON_RPCGMGR_CMD_NONE
} EJSON_RPCGMGR_CMD;
#define RPCMGR_RPC_TASK_STS_GET "get_rpc_req_status"
#define RPCMGR_RPC_TASK_STS_ARGID "taskId"
#define RPCMGR_RPC_TASK_STS_ARGSTS "taskStatus"
typedef struct RPCMGR_TASK_STS_PACKET_T {
  int taskID;
  char task_sts[512];
} RPCMGR_TASK_STS_PACKET;
#define RPCMGR_RPC_SER_VER_GET "get_rpc_srv_version"
#define RPCMGR_RPC_SER_VER_ARGVER "version"
typedef struct RPCMGR_INTEGER_PACKET_T {
  int value;
} RPCMGR_INTEGER_PACKET;
#define RPCMGR_RPC_TRIG_SAVE "trigger_settings_save"
#define RPCMGR_RPC_TRIG_SAVE_ARGID RPCMGR_RPC_TASK_STS_ARGID
#define RPCMGR_RPC_STTNG_STS_GET "get_settings_status"
#define RPCMGR_RPC_STTNG_STS_ARGSTS "status"
typedef struct RPCMGR_SETTINGS_STS_PACKET_T {
  char status[512];
} RPCMGR_SETTINGS_STS_PACKET;
#define RPCMGR_RPC_TRIG_SHUTDOWN "trigger_shutdown"
#define RPCMGR_RPC_TRIG_SHUTDOWN_ARGID RPCMGR_RPC_TASK_STS_ARGID
#define RPCMGR_RPC_RESET_TASKSTS "reset_task_status"
#define RPCMGR_RPC_READY_STS_GET "get_ready_status"
#define RPCMGR_RPC_READY_STS_ARGSTS "status"
#define RPCMGR_RPC_READY_STS_ARGSTS_TBL                                        \
  { "notready", "ready", "busy", "unknown", "none", "\0" }
typedef enum EJSON_RPCGMGR_READY_STATE_T {
  EJSON_RPCGMGR_READY_STATE_NOT_READY,
  EJSON_RPCGMGR_READY_STATE_READY,
  EJSON_RPCGMGR_READY_STATE_BUSY,
  EJSON_RPCGMGR_READY_STATE_UNKNOWN,
  EJSON_RPCGMGR_READY_STATE_NONE
} EJSON_RPCGMGR_READY_STATE;
typedef struct RPCMGR_READY_STS_PACKET_T {
  EJSON_RPCGMGR_READY_STATE status;
  char status_str[512];
} RPCMGR_READY_STS_PACKET;
#define RPCMGR_RPC_DEBUG_LOG_GET "get_debug_logging"
#define RPCMGR_RPC_DEBUG_LOG_SET "set_debug_logging"
#define RPCMGR_RPC_DEBUG_LOG_ARGSTS "status"
#define RPCMGR_RPC_DEBUG_LOG_ARGSTS_TBL                                        \
  { "disable", "enable", "unknown", "none", "\0" }
typedef enum EJSON_RPCGMGR_FLAG_STATE_T {
  EJSON_RPCGMGR_FLAG_STATE_DISABLE,
  EJSON_RPCGMGR_FLAG_STATE_ENABLE,
  EJSON_RPCGMGR_FLAG_STATE_UNKNOWN,
  EJSON_RPCGMGR_FLAG_STATE_NONE
} EJSON_RPCGMGR_FLAG_STATE;
typedef struct RPCMGR_DEBUG_LOG_PACKET_T {
  EJSON_RPCGMGR_FLAG_STATE status;
} RPCMGR_DEBUG_LOG_PACKET;
#define RPCMGR_RPC_DEV_TYPE_GET "get_device_type"
#define RPCMGR_RPC_DEV_TYPE_ARG "type"
typedef struct RPCMGR_DEV_TYPE_PACKET_T {
  char status_str[512];
} RPCMGR_DEV_TYPE_PACKET;
#define RPCMGR_RPC_TRIG_FACT_STORE "trigger_factory_store"
#define RPCMGR_RPC_TRIG_FACT_STORE_ARGID RPCMGR_RPC_TASK_STS_ARGID
#define RPCMGR_RPC_TRIG_FACT_RESTORE "trigger_factory_restore"
#define RPCMGR_RPC_TRIG_FACT_RESTORE_ARGID RPCMGR_RPC_TASK_STS_ARGID
typedef struct RPCMGR_EVENT_PACKET_T {
  int cltToken;
  int portNum;
  int eventNum;
  int srvToken;
  int sock_descr;
  int sock_id;
  char ip[512];
  int eventArg;
  int eventArg2;
} RPCMGR_EVENT_PACKET;
#define RPCMGR_RPC_TRIG_RUN "run_now"
#define RPCMGR_RPC_DEVOP_STATE_GET "get_devop_state"
#define RPCMGR_RPC_DEVOP_STATE_SET "set_devop_state"
#define RPCMGR_RPC_DEVOP_STATE_ARGSTS "status"
#define RPCMGR_RPC_DEVOP_STATE_ARGSTS_TBL                                      \
  {                                                                            \
    "idle", "on", "laststate", "off", "reboot", "booting", "rebooting",        \
        "switchingoff", "boot", "idlenoexthw", "unknown", "none", "\0"         \
  }
typedef enum EJSON_RPCGMGR_DEVOP_STATE_T {
  EJSON_RPCGMGR_DEVOP_STATE_IDLE,
  EJSON_RPCGMGR_DEVOP_STATE_ON,
  EJSON_RPCGMGR_DEVOP_STATE_LAST_STATE,
  EJSON_RPCGMGR_DEVOP_STATE_OFF,
  EJSON_RPCGMGR_DEVOP_STATE_REBOOT,
  EJSON_RPCGMGR_DEVOP_STATE_BOOTING,
  EJSON_RPCGMGR_DEVOP_STATE_REBOOTING,
  EJSON_RPCGMGR_DEVOP_STATE_SWITCHING_OFF,
  EJSON_RPCGMGR_DEVOP_STATE_BOOT,
  EJSON_RPCGMGR_DEVOP_STATE_IDLE_NOEXTHW,
  EJSON_RPCGMGR_DEVOP_STATE_UNKNOWN,
  EJSON_RPCGMGR_DEVOP_STATE_NONE
} EJSON_RPCGMGR_DEVOP_STATE;
typedef struct RPCMGR_DEVOP_STS_PACKET_T {
  EJSON_RPCGMGR_DEVOP_STATE status;
  char status_str[512];
} RPCMGR_DEVOP_STS_PACKET;
#define RPCMGR_RPC_MW_BYTE_GET "read_byte"
#define RPCMGR_RPC_MW_BYTE_SET "write_byte"
#define RPCMGR_RPC_MW_WORD_GET "read_word"
#define RPCMGR_RPC_MW_WORD_SET "write_word"
#define RPCMGR_RPC_MW_DWORD_GET "read_dword"
#define RPCMGR_RPC_MW_DWORD_SET "write_dword"
#define RPCMGR_RPC_MW_ARRAY_GET "read_array"
#define RPCMGR_RPC_MW_ARRAY_SET "write_array"
#define RPCMGR_RPC_MW_FILE_GET "read_file"
#define RPCMGR_RPC_MW_FILE_SET "write_file"
#define RPCMGR_RPC_MW_FILE_VERIFY "verify_file"
#define RPCMGR_RPC_MW_ARGADDR "addr"
#define RPCMGR_RPC_MW_ARGDATA "data"
#define RPCMGR_RPC_MW_ARGBUFF "buff"
#define RPCMGR_RPC_MW_ARGSIZE "size"
#define RPCMGR_RPC_MW_ARGOFFSET "offset"
#define RPCMGR_RPC_MW_ARGFILEPATH "filepath"
typedef struct RPCMGR_MIDDLEWARE_PACKET_T {
  uint32_t addr;
  uint32_t offset;
  uint8_t byte;
  uint16_t word;
  uint32_t dword;
  size_t size;
} RPCMGR_MIDDLEWARE_PACKET;
class ADJsonRpcMgrProducer;
class ADJsonRpcMgrConsumer {
  bool emulation;
  bool logmsg;
  bool cmnrpchandler;

public:
  int parent_index;
  int index;
  std::string strRpcName;
  std::string GetRpcName() { return strRpcName; };
  int GetRpcParentIndex() { return parent_index; };
  bool get_debug_log_flag() { return logmsg; };
  int set_debug_log_flag(bool flag) {
    logmsg = flag;
    return 0;
  };
  bool get_emulation_flag() { return emulation; };
  bool get_cmn_rpc_handler_flag() { return cmnrpchandler; };
  ADJsonRpcMgrConsumer(std::string rpc_name, int consumer_index, bool emu,
                       bool log, bool cmnhandler = false) {
    strRpcName = rpc_name;
    index = consumer_index;
    emulation = emu;
    logmsg = log;
    cmnrpchandler = cmnhandler;
  };
  virtual ~ADJsonRpcMgrConsumer(){};
  virtual int MapJsonToBinary(JsonDataCommObj *pReq, int index) = 0;
  virtual int MapBinaryToJson(JsonDataCommObj *pReq, int index) = 0;
  virtual int ProcessWork(JsonDataCommObj *pReq, int index,
                          ADJsonRpcMgrProducer *pObj) = 0;
  virtual RPC_SRV_RESULT ProcessWorkAsync(int index,
                                          unsigned char *pWorkData) = 0;
  virtual void ReceiveEvent(int cltToken, int evntNum, int evntArg,
                            int evntArg2) = 0;
};
// rpclist and eventReceiver are appended by the service (possibly after
// Start()) and read by the RPC, task and event threads: all access holds
// list_lock. Entries are never removed, so returned pointers stay valid.
class ADJsonRpcMgrProducer {
  std::vector<ADJsonRpcMgrConsumer *> rpclist;
  std::vector<ADJsonRpcMgrConsumer *> eventReceiver;
  pthread_mutex_t list_lock;
  struct ListGuard {
    pthread_mutex_t *m;
    explicit ListGuard(pthread_mutex_t *mtx) : m(mtx) { pthread_mutex_lock(m); }
    ~ListGuard() { pthread_mutex_unlock(m); }
  };

protected:
  ADTaskWorker AsyncTaskWorker;
  ADEvntMgr EventMgr;
  void notify_event_receivers(int cltToken, int evntNum, int evntArg,
                              int evntArg2) {
    std::vector<ADJsonRpcMgrConsumer *> receivers;
    {
      ListGuard g(&list_lock);
      receivers = eventReceiver;
    }
    std::vector<ADJsonRpcMgrConsumer *>::iterator iter;
    for (iter = receivers.begin(); iter != receivers.end(); ++iter)
      (*iter)->ReceiveEvent(cltToken, evntNum, evntArg, evntArg2);
  }
  ADJsonRpcMgrConsumer *getRpcHandler(std::string rpcName) {
    ListGuard g(&list_lock);
    std::vector<ADJsonRpcMgrConsumer *>::iterator iter;
    for (iter = rpclist.begin(); iter != rpclist.end(); ++iter) {
      if ((*iter)->GetRpcName() == rpcName)
        return (*iter);
    }
    return NULL;
  }
  ADJsonRpcMgrConsumer *getRpcHandler(int index) {
    ListGuard g(&list_lock);
    if (index < 0 || index >= (int)rpclist.size())
      return NULL;
    return rpclist[index];
  }
  ADJsonRpcMgrConsumer *getRpcHandlerByParentIndex(int index) {
    ListGuard g(&list_lock);
    std::vector<ADJsonRpcMgrConsumer *>::iterator iter;
    for (iter = rpclist.begin(); iter != rpclist.end(); ++iter) {
      if ((*iter)->GetRpcParentIndex() == index)
        return (*iter);
    }
    return NULL;
  }
  int setRpcDebugLogFlag(bool debuglog) {
    ListGuard g(&list_lock);
    std::vector<ADJsonRpcMgrConsumer *>::iterator iter;
    for (iter = rpclist.begin(); iter != rpclist.end(); ++iter)
      (*iter)->set_debug_log_flag(debuglog);
    return 0;
  }
  ADJsonRpcMgrConsumer *getCmnRpcHandler() {
    ListGuard g(&list_lock);
    std::vector<ADJsonRpcMgrConsumer *>::iterator iter;
    for (iter = rpclist.begin(); iter != rpclist.end(); ++iter) {
      if ((*iter)->get_cmn_rpc_handler_flag() == true)
        return (*iter);
    }
    return NULL;
  }

public:
  ADJsonRpcMgrProducer() { pthread_mutex_init(&list_lock, NULL); }
  virtual ~ADJsonRpcMgrProducer() { pthread_mutex_destroy(&list_lock); };
  // RPC handlers are registered as methods in Start(): attach them before.
  void AttachRpc(ADJsonRpcMgrConsumer *pRpc) {
    ListGuard g(&list_lock);
    rpclist.push_back(pRpc);
  }
  void AttachEventReceiver(ADJsonRpcMgrConsumer *pReceiver) {
    ListGuard g(&list_lock);
    eventReceiver.push_back(pReceiver);
  }
  int get_total_attached_rpcs() {
    ListGuard g(&list_lock);
    return rpclist.size();
  }
  // true once Stop() began; long ProcessWorkAsync() work should poll it
  bool IsStopping() { return AsyncTaskWorker.stop_requested(); }
  int MapJsonToBinary(JsonDataCommObj *pReq) {
    if ((pReq->cmd_index - EJSON_RPCGMGR_CMD_END) >= get_total_attached_rpcs())
      return -1;
    if ((pReq->cmd_index - EJSON_RPCGMGR_CMD_END) < 0)
      return -1;
    ADJsonRpcMgrConsumer *pPageHandler =
        getRpcHandler(pReq->cmd_index - EJSON_RPCGMGR_CMD_END);
    if (pPageHandler == NULL)
      return -1;
    else
      return pPageHandler->MapJsonToBinary(pReq, pPageHandler->index);
  }
  int MapBinaryToJson(JsonDataCommObj *pReq) {
    if ((pReq->cmd_index - EJSON_RPCGMGR_CMD_END) >= get_total_attached_rpcs())
      return -1;
    if ((pReq->cmd_index - EJSON_RPCGMGR_CMD_END) < 0)
      return -1;
    ADJsonRpcMgrConsumer *pPageHandler =
        getRpcHandler(pReq->cmd_index - EJSON_RPCGMGR_CMD_END);
    if (pPageHandler == NULL)
      return -1;
    else
      return pPageHandler->MapBinaryToJson(pReq, pPageHandler->index);
  }
  int ProcessWork(JsonDataCommObj *pReq) {
    if ((pReq->cmd_index - EJSON_RPCGMGR_CMD_END) >= get_total_attached_rpcs())
      return -1;
    if ((pReq->cmd_index - EJSON_RPCGMGR_CMD_END) < 0)
      return -1;
    ADJsonRpcMgrConsumer *pPageHandler =
        getRpcHandler(pReq->cmd_index - EJSON_RPCGMGR_CMD_END);
    if (pPageHandler == NULL)
      return -1;
    else
      return pPageHandler->ProcessWork(pReq, pPageHandler->index, this);
  }
  RPC_SRV_RESULT AsyncTaskWork(int cmd, unsigned char *pWorkData) {
    if ((cmd - EJSON_RPCGMGR_CMD_END) >= get_total_attached_rpcs())
      return RPC_SRV_RESULT_FAIL;
    if ((cmd - EJSON_RPCGMGR_CMD_END) < 0)
      return RPC_SRV_RESULT_FAIL;
    ADJsonRpcMgrConsumer *pPageHandler =
        getRpcHandler(cmd - EJSON_RPCGMGR_CMD_END);
    if (pPageHandler == NULL)
      return RPC_SRV_RESULT_FAIL;
    else {
      return pPageHandler->ProcessWorkAsync(cmd - EJSON_RPCGMGR_CMD_END,
                                            pWorkData);
    }
  }
  RPC_SRV_RESULT PushAsyncTask(int cmd, unsigned char *pWorkData, int *taskID,
                               WORK_CMD_AFTER_DONE done_flag) {
    if (AsyncTaskWorker.push_task(cmd + EJSON_RPCGMGR_CMD_END, pWorkData,
                                  taskID, done_flag) == 0)
      return RPC_SRV_RESULT_IN_PROG;
    else
      return RPC_SRV_RESULT_FAIL;
  }
  void TaskWorkerSetPortNumber(int port) {
    AsyncTaskWorker.notifyPortNum = port;
  }
  RPC_SRV_RESULT CmnRpcHandler(int cmd, unsigned char *pWorkData) {
    ADJsonRpcMgrConsumer *pPageHandler = getCmnRpcHandler();
    if (pPageHandler == NULL)
      return RPC_SRV_RESULT_FEATURE_UNSUPPORTED;
    else
      return pPageHandler->ProcessWorkAsync(cmd, pWorkData);
  }
  int ProcessWorkCmnRpc(JsonDataCommObj *pReq) {
    ADJsonRpcMgrConsumer *pPageHandler = getCmnRpcHandler();
    if (pPageHandler == NULL)
      return -1;
    else
      return pPageHandler->ProcessWork(pReq, pReq->cmd_index, this);
  }
};
class ADJsonRpcMgr : public ADJsonRpcMgrProducer,
                     public ADJsonRpcMapConsumer,
                     public ADTaskWorkerConsumer,
                     public ADTaskWorkerEventSink,
                     public ADTimerConsumer,
                     public ADCmnStringProcessor,
                     public ADEvntMgrConsumer {
  ADTimer *myTimer;
  bool stopped;
  bool methods_attached;     // Start() registered the RPC methods
  pthread_mutex_t stop_lock; // Stop() returns only when everything stopped
  ADJsonRpcProxy Proxy;
  ADJsonRpcMapper JMapper;
  int svnVersion;
  ADCMN_DEV_INFO *pDevInfo;
  bool shutdown_support;
  EJSON_RPCGMGR_READY_STATE ServiceReadyFlag;
  bool ServiceDebugFlag;
  virtual int process_json_to_binary(JsonDataCommObj *pReq);
  virtual int process_binary_to_json(JsonDataCommObj *pReq);
  virtual int process_work(JsonDataCommObj *pReq);
  virtual RPC_SRV_RESULT run_work(int cmd, unsigned char *pWorkData,
                                  ADTaskWorkerProducer *pTaskWorker);
  virtual int sigio_notification() { return 0; };
  virtual int timer_notification() { return 0; };
  virtual int custom_sig_notification(int signum) { return 0; };
  virtual int receive_events(int cltToken, int evntNum, int evntArg,
                             int evntArg2);
  virtual void task_worker_event(int evntNum, int evntArg, int evntArg2);
  int MyMapJsonToBinary(JsonDataCommObj *pReq);
  int MyMapBinaryToJson(JsonDataCommObj *pReq);
  int MyProcessWork(JsonDataCommObj *pReq);
  int create_req_obj(JsonDataCommObj *pReq);
  int delete_req_obj(JsonDataCommObj *pReq);
  int json_to_bin_get_task_sts(JsonDataCommObj *pReq);
  int process_get_task_status(RPC_SRV_REQ *pReq);
  int bin_to_json_get_task_sts(JsonDataCommObj *pReq);
  int json_to_bin_get_rpc_srv_version(JsonDataCommObj *pReq);
  int process_rpc_server_version(RPC_SRV_REQ *pReq);
  int bin_to_json_get_rpc_srv_version(JsonDataCommObj *pReq);
  int json_to_bin_trigger_datasave(JsonDataCommObj *pReq);
  int process_trigger_datasave(RPC_SRV_REQ *pReq);
  int bin_to_json_trigger_datasave(JsonDataCommObj *pReq);
  int json_to_bin_get_settings_sts(JsonDataCommObj *pReq);
  int process_get_settings_status(RPC_SRV_REQ *pReq, JsonDataCommObj *pReqObj);
  int bin_to_json_get_settings_sts(JsonDataCommObj *pReq);
  int json_to_bin_shutdown_service(JsonDataCommObj *pReq);
  int process_shutdown_service(RPC_SRV_REQ *pReq);
  int bin_to_json_shutdown_service(JsonDataCommObj *pReq);
  int json_to_bin_reset_task_sts(JsonDataCommObj *pReq);
  int process_reset_task_sts(RPC_SRV_REQ *pReq);
  int bin_to_json_reset_task_sts(JsonDataCommObj *pReq);
  int json_to_bin_get_ready_sts(JsonDataCommObj *pReq);
  int process_get_ready_status(RPC_SRV_REQ *pReq);
  int bin_to_json_get_ready_sts(JsonDataCommObj *pReq);
  int json_to_bin_get_debug_logging(JsonDataCommObj *pReq);
  int process_get_debug_logging(RPC_SRV_REQ *pReq);
  int bin_to_json_get_debug_logging(JsonDataCommObj *pReq);
  int json_to_bin_set_debug_logging(JsonDataCommObj *pReq);
  int process_set_debug_logging(RPC_SRV_REQ *pReq);
  int bin_to_json_set_debug_logging(JsonDataCommObj *pReq);
  int json_to_bin_trigger_factory_store(JsonDataCommObj *pReq);
  int process_trigger_factory_store(RPC_SRV_REQ *pReq);
  int bin_to_json_trigger_factory_store(JsonDataCommObj *pReq);
  int json_to_bin_trigger_factory_restore(JsonDataCommObj *pReq);
  int process_trigger_factory_restore(RPC_SRV_REQ *pReq);
  int bin_to_json_trigger_factory_restore(JsonDataCommObj *pReq);
  int json_to_bin_event_subscribe(JsonDataCommObj *pReq);
  int process_event_subscribe(RPC_SRV_REQ *pReq);
  int bin_to_json_event_subscribe(JsonDataCommObj *pReq);
  int json_to_bin_event_unsubscribe(JsonDataCommObj *pReq);
  int process_event_unsubscribe(RPC_SRV_REQ *pReq);
  int bin_to_json_event_unsubscribe(JsonDataCommObj *pReq);
  int json_to_bin_event_notify(JsonDataCommObj *pReq);
  int process_event_notify(RPC_SRV_REQ *pReq);
  int bin_to_json_event_notify(JsonDataCommObj *pReq);
  int json_to_bin_event_process(JsonDataCommObj *pReq);
  int process_event_process(RPC_SRV_REQ *pReq);
  int bin_to_json_event_process(JsonDataCommObj *pReq);
  int json_to_bin_trigger_run(JsonDataCommObj *pReq);
  int process_trigger_run(RPC_SRV_REQ *pReq);
  int bin_to_json_trigger_run(JsonDataCommObj *pReq);
  int json_to_bin_get_devop_state(JsonDataCommObj *pReq);
  int process_get_devop_state(RPC_SRV_REQ *pReq, JsonDataCommObj *pReqObj);
  int bin_to_json_get_devop_state(JsonDataCommObj *pReq);
  int json_to_bin_set_devop_state(JsonDataCommObj *pReq);
  int process_set_devop_state(RPC_SRV_REQ *pReq);
  int bin_to_json_set_devop_state(JsonDataCommObj *pReq);
  int json_to_bin_get_mw_byte(JsonDataCommObj *pReq);
  int process_get_mw_byte(RPC_SRV_REQ *pReq, JsonDataCommObj *pReqObj);
  int bin_to_json_get_mw_byte(JsonDataCommObj *pReq);
  int json_to_bin_set_mw_byte(JsonDataCommObj *pReq);
  int process_set_mw_byte(RPC_SRV_REQ *pReq, JsonDataCommObj *pReqObj);
  int bin_to_json_set_mw_byte(JsonDataCommObj *pReq);
  int json_to_bin_get_mw_word(JsonDataCommObj *pReq);
  int process_get_mw_word(RPC_SRV_REQ *pReq, JsonDataCommObj *pReqObj);
  int bin_to_json_get_mw_word(JsonDataCommObj *pReq);
  int json_to_bin_set_mw_word(JsonDataCommObj *pReq);
  int process_set_mw_word(RPC_SRV_REQ *pReq, JsonDataCommObj *pReqObj);
  int bin_to_json_set_mw_word(JsonDataCommObj *pReq);
  int json_to_bin_get_mw_dword(JsonDataCommObj *pReq);
  int process_get_mw_dword(RPC_SRV_REQ *pReq, JsonDataCommObj *pReqObj);
  int bin_to_json_get_mw_dword(JsonDataCommObj *pReq);
  int json_to_bin_set_mw_dword(JsonDataCommObj *pReq);
  int process_set_mw_dword(RPC_SRV_REQ *pReq, JsonDataCommObj *pReqObj);
  int bin_to_json_set_mw_dword(JsonDataCommObj *pReq);

public:
  ADJsonRpcMgr(int ver, bool debuglog = false, ADCMN_DEV_INFO *pDev = NULL);
  ~ADJsonRpcMgr();
  int AttachHeartBeat(ADTimer *pTimer);
  // Returns 0, or -1 if the port cannot be opened (the reason is logged);
  // a service should then exit with an error so its supervisor restarts it.
  // Start() may be called again after a failure (it then only retries
  // listening); after Stop() the object cannot be started again.
  int Start(int port, int socket_log, int emulation);
  // same, listening only on bind_ip (e.g. "127.0.0.1")
  int Start(int port, int socket_log, int emulation, const char *bind_ip);
  // Stops all threads in pipeline order (network input, RPC handlers, task
  // worker, event threads, responses). Idempotent; also called by the
  // destructor before any member is destroyed (finding H2).
  // RULE: call Stop() before any object attached with AttachRpc(),
  // AttachEventReceiver() or AttachHeartBeat() is destroyed. Objects declared
  // after the manager in main() are destroyed before it, while the manager's
  // threads still call them (finding V2-C1).
  void Stop();
  int SupportShutdownRpc(bool support);
  int SetServiceReadyFlag(EJSON_RPCGMGR_READY_STATE sts);
};
#endif
