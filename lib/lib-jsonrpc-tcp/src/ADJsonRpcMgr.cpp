#include "ADJsonRpcMgr.hpp"
#include "ADCmnPortList.h"
ADJsonRpcMgr::ADJsonRpcMgr(int ver, bool debuglog, ADCMN_DEV_INFO *pDev) {
  pDevInfo = pDev;
  svnVersion = ver;
  ServiceDebugFlag = debuglog;
  JMapper.AttachProxy(&Proxy);
  JMapper.AttachMapper(this);
  JMapper.AttachWorker(this);
  AsyncTaskWorker.attach_helper(this);
  EventMgr.AttachReceiver(this);
  shutdown_support = true;
  ServiceReadyFlag = EJSON_RPCGMGR_READY_STATE_NOT_READY;
}
ADJsonRpcMgr::~ADJsonRpcMgr() {}
int ADJsonRpcMgr::AttachHeartBeat(ADTimer *pTimer) {
  pTimer->subscribe_timer_notification(this);
  myTimer = pTimer;
  return 0;
}
int ADJsonRpcMgr::SupportShutdownRpc(bool support) {
  shutdown_support = support;
  return 0;
}
int ADJsonRpcMgr::SetServiceReadyFlag(EJSON_RPCGMGR_READY_STATE sts) {
  ServiceReadyFlag = sts;
  return 0;
}
int ADJsonRpcMgr::receive_events(int cltToken, int evntNum, int evntArg,
                                 int evntArg2) {
  notify_event_receivers(cltToken, evntNum, evntArg, evntArg2);
  return 0;
}
RPC_SRV_RESULT ADJsonRpcMgr::run_work(int cmd, unsigned char *pWorkData,
                                      ADTaskWorkerProducer *pTaskWorker) {
  RPC_SRV_RESULT ret_val = RPC_SRV_RESULT_FAIL;
  if (cmd >= EJSON_RPCGMGR_CMD_END)
    ret_val = AsyncTaskWork(cmd, pWorkData);
  else {
    switch (cmd) {
    case EJSON_RPCGMGR_SHUTDOWN_SERVICE: {
      RPCMGR_TASK_STS_PACKET *pPacket;
      pPacket = (RPCMGR_TASK_STS_PACKET *)pWorkData;
      myTimer->forced_exit();
      OBJ_MEM_DELETE(pWorkData);
      ret_val = RPC_SRV_RESULT_SUCCESS;
    } break;
    case EJSON_RPCGMGR_TRIGGER_DATASAVE: {
      RPCMGR_TASK_STS_PACKET *pPacket;
      pPacket = (RPCMGR_TASK_STS_PACKET *)pWorkData;
      ret_val = CmnRpcHandler(cmd, pWorkData);
      OBJ_MEM_DELETE(pWorkData);
    } break;
    case EJSON_RPCGMGR_TRIGGER_FACTORY_STORE: {
      RPCMGR_TASK_STS_PACKET *pPacket;
      pPacket = (RPCMGR_TASK_STS_PACKET *)pWorkData;
      ret_val = CmnRpcHandler(cmd, pWorkData);
      OBJ_MEM_DELETE(pWorkData);
    } break;
    case EJSON_RPCGMGR_TRIGGER_FACTORY_RESTORE: {
      RPCMGR_TASK_STS_PACKET *pPacket;
      pPacket = (RPCMGR_TASK_STS_PACKET *)pWorkData;
      ret_val = CmnRpcHandler(cmd, pWorkData);
      OBJ_MEM_DELETE(pWorkData);
    } break;
    case EJSON_RPCGMGR_TRIGGER_RUN: {
      RPCMGR_TASK_STS_PACKET *pPacket;
      pPacket = (RPCMGR_TASK_STS_PACKET *)pWorkData;
      ret_val = CmnRpcHandler(cmd, pWorkData);
      OBJ_MEM_DELETE(pWorkData);
    } break;
    case EJSON_RPCMGR_SET_DEVOP_STATE: {
      RPCMGR_TASK_STS_PACKET *pPacket;
      pPacket = (RPCMGR_TASK_STS_PACKET *)pWorkData;
      ret_val = CmnRpcHandler(cmd, pWorkData);
      OBJ_MEM_DELETE(pWorkData);
    } break;
    default:
      break;
    }
  }
  return ret_val;
}
int ADJsonRpcMgr::Start(int port, int socket_log, int emulation) {
  char rpc_name[1024];
  Proxy.start_listening(port, socket_log);
  TaskWorkerSetPortNumber(port);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_GET_TASK_STS,
                            (char *)RPCMGR_RPC_TASK_STS_GET);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_GET_RPC_SRV_VERSION,
                            (char *)RPCMGR_RPC_SER_VER_GET);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_TRIGGER_DATASAVE,
                            (char *)RPCMGR_RPC_TRIG_SAVE);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_GET_SETTINGS_STS,
                            (char *)RPCMGR_RPC_STTNG_STS_GET);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_SHUTDOWN_SERVICE,
                            (char *)RPCMGR_RPC_TRIG_SHUTDOWN);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_RESET_TASK_STS,
                            (char *)RPCMGR_RPC_RESET_TASKSTS);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_GET_READY_STS,
                            (char *)RPCMGR_RPC_READY_STS_GET);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_GET_DEBUG_LOG,
                            (char *)RPCMGR_RPC_DEBUG_LOG_GET);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_SET_DEBUG_LOG,
                            (char *)RPCMGR_RPC_DEBUG_LOG_SET);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_TRIGGER_FACTORY_STORE,
                            (char *)RPCMGR_RPC_TRIG_FACT_STORE);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_TRIGGER_FACTORY_RESTORE,
                            (char *)RPCMGR_RPC_TRIG_FACT_RESTORE);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_EVENT_SUBSCRIBE,
                            (char *)RPCMGR_RPC_EVENT_SUBSCRIBE);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_EVENT_UNSUBSCRIBE,
                            (char *)RPCMGR_RPC_EVENT_UNSUBSCRIBE);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_EVENT_NOTIFY,
                            (char *)RPCMGR_RPC_EVENT_NOTIFY);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_EVENT_PROCESS,
                            (char *)RPCMGR_RPC_EVENT_PROCESS);
  JMapper.attach_rpc_method(EJSON_RPCGMGR_TRIGGER_RUN,
                            (char *)RPCMGR_RPC_TRIG_RUN);
  JMapper.attach_rpc_method(EJSON_RPCMGR_GET_DEVOP_STATE,
                            (char *)RPCMGR_RPC_DEVOP_STATE_GET);
  JMapper.attach_rpc_method(EJSON_RPCMGR_SET_DEVOP_STATE,
                            (char *)RPCMGR_RPC_DEVOP_STATE_SET);
  JMapper.attach_rpc_method(EJSON_RPCMGR_GET_MW_BYTE,
                            (char *)RPCMGR_RPC_MW_BYTE_GET);
  JMapper.attach_rpc_method(EJSON_RPCMGR_SET_MW_BYTE,
                            (char *)RPCMGR_RPC_MW_BYTE_SET);
  JMapper.attach_rpc_method(EJSON_RPCMGR_GET_MW_WORD,
                            (char *)RPCMGR_RPC_MW_WORD_GET);
  JMapper.attach_rpc_method(EJSON_RPCMGR_SET_MW_WORD,
                            (char *)RPCMGR_RPC_MW_WORD_SET);
  JMapper.attach_rpc_method(EJSON_RPCMGR_GET_MW_DWORD,
                            (char *)RPCMGR_RPC_MW_DWORD_GET);
  JMapper.attach_rpc_method(EJSON_RPCMGR_SET_MW_DWORD,
                            (char *)RPCMGR_RPC_MW_DWORD_SET);
  int total = get_total_attached_rpcs();
  for (int i = 0; i < total; i++) {
    ADJsonRpcMgrConsumer *rpc = getRpcHandler(i);
    strcpy(rpc_name, rpc->strRpcName.c_str());
    rpc->parent_index = i + EJSON_RPCGMGR_CMD_END;
    JMapper.attach_rpc_method(i + EJSON_RPCGMGR_CMD_END, rpc_name);
  }
  return 0;
}
int ADJsonRpcMgr::create_req_obj(JsonDataCommObj *pReq) {
  RPC_SRV_REQ *pPanelReq = NULL;
  OBJECT_MEM_NEW(pPanelReq, RPC_SRV_REQ);
  if (pPanelReq == NULL)
    return -1;
  pPanelReq->clientId = pReq->req_id;
  pPanelReq->reqIdent = 0;
  pPanelReq->result = RPC_SRV_RESULT_NOT_STARTED;
  pPanelReq->dataSize = 0;
  pPanelReq->dataRef = NULL;
  pReq->dataObjLen = sizeof(RPC_SRV_REQ);
  pReq->pDataObj = (unsigned char *)pPanelReq;
  return 0;
}
int ADJsonRpcMgr::delete_req_obj(JsonDataCommObj *pReq) {
  if (pReq->pDataObj == NULL)
    return 0;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  OBJ_MEM_DELETE(pPanelReq);
  return 0;
}
int ADJsonRpcMgr::process_json_to_binary(JsonDataCommObj *pReq) {
  int result = -1;
  if (create_req_obj(pReq) != 0)
    return -1;
  EJSON_RPCGMGR_CMD rpc = (EJSON_RPCGMGR_CMD)pReq->cmd_index;
  if (rpc >= EJSON_RPCGMGR_CMD_END)
    result = MapJsonToBinary(pReq);
  else
    result = MyMapJsonToBinary(pReq);
  if (result != 0)
    delete_req_obj(pReq);
  return result;
}
int ADJsonRpcMgr::process_binary_to_json(JsonDataCommObj *pReq) {
  int result = -1;
  EJSON_RPCGMGR_CMD rpc = (EJSON_RPCGMGR_CMD)pReq->cmd_index;
  if (rpc >= EJSON_RPCGMGR_CMD_END)
    result = MapBinaryToJson(pReq);
  else
    result = MyMapBinaryToJson(pReq);
  delete_req_obj(pReq);
  return result;
}
int ADJsonRpcMgr::process_work(JsonDataCommObj *pReq) {
  EJSON_RPCGMGR_CMD rpc = (EJSON_RPCGMGR_CMD)pReq->cmd_index;
  if (rpc >= EJSON_RPCGMGR_CMD_END)
    return ProcessWork(pReq);
  else
    return MyProcessWork(pReq);
}
int ADJsonRpcMgr::MyMapJsonToBinary(JsonDataCommObj *pReq) {
  int result = -1;
  EJSON_RPCGMGR_CMD rpc = (EJSON_RPCGMGR_CMD)pReq->cmd_index;
  switch (rpc) {
  case EJSON_RPCGMGR_GET_TASK_STS:
    result = json_to_bin_get_task_sts(pReq);
    break;
  case EJSON_RPCGMGR_GET_RPC_SRV_VERSION:
    result = json_to_bin_get_rpc_srv_version(pReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_DATASAVE:
    result = json_to_bin_trigger_datasave(pReq);
    break;
  case EJSON_RPCGMGR_GET_SETTINGS_STS:
    result = json_to_bin_get_settings_sts(pReq);
    break;
  case EJSON_RPCGMGR_SHUTDOWN_SERVICE:
    result = json_to_bin_shutdown_service(pReq);
    break;
  case EJSON_RPCGMGR_RESET_TASK_STS:
    result = json_to_bin_reset_task_sts(pReq);
    break;
  case EJSON_RPCGMGR_GET_READY_STS:
    result = json_to_bin_get_ready_sts(pReq);
    break;
  case EJSON_RPCGMGR_GET_DEBUG_LOG:
    result = json_to_bin_get_debug_logging(pReq);
    break;
  case EJSON_RPCGMGR_SET_DEBUG_LOG:
    result = json_to_bin_set_debug_logging(pReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_FACTORY_STORE:
    result = json_to_bin_trigger_factory_store(pReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_FACTORY_RESTORE:
    result = json_to_bin_trigger_factory_restore(pReq);
    break;
  case EJSON_RPCGMGR_EVENT_SUBSCRIBE:
    result = json_to_bin_event_subscribe(pReq);
    break;
  case EJSON_RPCGMGR_EVENT_UNSUBSCRIBE:
    result = json_to_bin_event_unsubscribe(pReq);
    break;
  case EJSON_RPCGMGR_EVENT_NOTIFY:
    result = json_to_bin_event_notify(pReq);
    break;
  case EJSON_RPCGMGR_EVENT_PROCESS:
    result = json_to_bin_event_process(pReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_RUN:
    result = json_to_bin_trigger_run(pReq);
    break;
  case EJSON_RPCMGR_GET_DEVOP_STATE:
    result = json_to_bin_get_devop_state(pReq);
    break;
  case EJSON_RPCMGR_SET_DEVOP_STATE:
    result = json_to_bin_set_devop_state(pReq);
    break;
  case EJSON_RPCMGR_GET_MW_BYTE:
    result = json_to_bin_get_mw_byte(pReq);
    break;
  case EJSON_RPCMGR_SET_MW_BYTE:
    result = json_to_bin_set_mw_byte(pReq);
    break;
  case EJSON_RPCMGR_GET_MW_WORD:
    result = json_to_bin_get_mw_word(pReq);
    break;
  case EJSON_RPCMGR_SET_MW_WORD:
    result = json_to_bin_set_mw_word(pReq);
    break;
  case EJSON_RPCMGR_GET_MW_DWORD:
    result = json_to_bin_get_mw_dword(pReq);
    break;
  case EJSON_RPCMGR_SET_MW_DWORD:
    result = json_to_bin_set_mw_dword(pReq);
    break;
  default:
    break;
  }
  return result;
}
int ADJsonRpcMgr::MyMapBinaryToJson(JsonDataCommObj *pReq) {
  int result = -1;
  EJSON_RPCGMGR_CMD rpc = (EJSON_RPCGMGR_CMD)pReq->cmd_index;
  switch (rpc) {
  case EJSON_RPCGMGR_GET_TASK_STS:
    result = bin_to_json_get_task_sts(pReq);
    break;
  case EJSON_RPCGMGR_GET_RPC_SRV_VERSION:
    result = bin_to_json_get_rpc_srv_version(pReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_DATASAVE:
    result = bin_to_json_trigger_datasave(pReq);
    break;
  case EJSON_RPCGMGR_GET_SETTINGS_STS:
    result = bin_to_json_get_settings_sts(pReq);
    break;
  case EJSON_RPCGMGR_SHUTDOWN_SERVICE:
    result = bin_to_json_shutdown_service(pReq);
    break;
  case EJSON_RPCGMGR_RESET_TASK_STS:
    result = bin_to_json_reset_task_sts(pReq);
    break;
  case EJSON_RPCGMGR_GET_READY_STS:
    result = bin_to_json_get_ready_sts(pReq);
    break;
  case EJSON_RPCGMGR_GET_DEBUG_LOG:
    result = bin_to_json_get_debug_logging(pReq);
    break;
  case EJSON_RPCGMGR_SET_DEBUG_LOG:
    result = bin_to_json_set_debug_logging(pReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_FACTORY_STORE:
    result = bin_to_json_trigger_factory_store(pReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_FACTORY_RESTORE:
    result = bin_to_json_trigger_factory_restore(pReq);
    break;
  case EJSON_RPCGMGR_EVENT_SUBSCRIBE:
    result = bin_to_json_event_subscribe(pReq);
    break;
  case EJSON_RPCGMGR_EVENT_UNSUBSCRIBE:
    result = bin_to_json_event_unsubscribe(pReq);
    break;
  case EJSON_RPCGMGR_EVENT_NOTIFY:
    result = bin_to_json_event_notify(pReq);
    break;
  case EJSON_RPCGMGR_EVENT_PROCESS:
    result = bin_to_json_event_process(pReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_RUN:
    result = bin_to_json_trigger_run(pReq);
    break;
  case EJSON_RPCMGR_GET_DEVOP_STATE:
    result = bin_to_json_get_devop_state(pReq);
    break;
  case EJSON_RPCMGR_SET_DEVOP_STATE:
    result = bin_to_json_set_devop_state(pReq);
    break;
  case EJSON_RPCMGR_GET_MW_BYTE:
    result = bin_to_json_get_mw_byte(pReq);
    break;
  case EJSON_RPCMGR_SET_MW_BYTE:
    result = bin_to_json_set_mw_byte(pReq);
    break;
  case EJSON_RPCMGR_GET_MW_WORD:
    result = bin_to_json_get_mw_word(pReq);
    break;
  case EJSON_RPCMGR_SET_MW_WORD:
    result = bin_to_json_set_mw_word(pReq);
    break;
  case EJSON_RPCMGR_GET_MW_DWORD:
    result = bin_to_json_get_mw_dword(pReq);
    break;
  case EJSON_RPCMGR_SET_MW_DWORD:
    result = bin_to_json_set_mw_dword(pReq);
    break;
  default:
    break;
  }
  return result;
}
int ADJsonRpcMgr::MyProcessWork(JsonDataCommObj *pReq) {
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  EJSON_RPCGMGR_CMD rpc = (EJSON_RPCGMGR_CMD)pReq->cmd_index;
  switch (rpc) {
  case EJSON_RPCGMGR_GET_TASK_STS:
    return process_get_task_status(pPanelReq);
    break;
  case EJSON_RPCGMGR_GET_RPC_SRV_VERSION:
    return process_rpc_server_version(pPanelReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_DATASAVE:
    return process_trigger_datasave(pPanelReq);
    break;
  case EJSON_RPCGMGR_GET_SETTINGS_STS:
    return process_get_settings_status(pPanelReq, pReq);
    break;
  case EJSON_RPCGMGR_SHUTDOWN_SERVICE:
    return process_shutdown_service(pPanelReq);
    break;
  case EJSON_RPCGMGR_RESET_TASK_STS:
    return process_reset_task_sts(pPanelReq);
    break;
  case EJSON_RPCGMGR_GET_READY_STS:
    return process_get_ready_status(pPanelReq);
    break;
  case EJSON_RPCGMGR_GET_DEBUG_LOG:
    return process_get_debug_logging(pPanelReq);
    break;
  case EJSON_RPCGMGR_SET_DEBUG_LOG:
    return process_set_debug_logging(pPanelReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_FACTORY_STORE:
    return process_trigger_factory_store(pPanelReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_FACTORY_RESTORE:
    return process_trigger_factory_restore(pPanelReq);
    break;
  case EJSON_RPCGMGR_EVENT_SUBSCRIBE:
    return process_event_subscribe(pPanelReq);
    break;
  case EJSON_RPCGMGR_EVENT_UNSUBSCRIBE:
    return process_event_unsubscribe(pPanelReq);
    break;
  case EJSON_RPCGMGR_EVENT_NOTIFY:
    return process_event_notify(pPanelReq);
    break;
  case EJSON_RPCGMGR_EVENT_PROCESS:
    return process_event_process(pPanelReq);
    break;
  case EJSON_RPCGMGR_TRIGGER_RUN:
    return process_trigger_run(pPanelReq);
    break;
  case EJSON_RPCMGR_GET_DEVOP_STATE:
    return process_get_devop_state(pPanelReq, pReq);
    break;
  case EJSON_RPCMGR_SET_DEVOP_STATE:
    return process_set_devop_state(pPanelReq);
    break;
  case EJSON_RPCMGR_GET_MW_BYTE:
    return process_get_mw_byte(pPanelReq, pReq);
    break;
  case EJSON_RPCMGR_SET_MW_BYTE:
    return process_set_mw_byte(pPanelReq, pReq);
    break;
  case EJSON_RPCMGR_GET_MW_WORD:
    return process_get_mw_word(pPanelReq, pReq);
    break;
  case EJSON_RPCMGR_SET_MW_WORD:
    return process_set_mw_word(pPanelReq, pReq);
    break;
  case EJSON_RPCMGR_GET_MW_DWORD:
    return process_get_mw_dword(pPanelReq, pReq);
    break;
  case EJSON_RPCMGR_SET_MW_DWORD:
    return process_set_mw_dword(pPanelReq, pReq);
    break;
  default:
    break;
  }
  return -1;
}
int ADJsonRpcMgr::json_to_bin_get_task_sts(JsonDataCommObj *pReq) {
  RPCMGR_TASK_STS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET, RPC_SRV_ACT_READ,
                       EJSON_RPCGMGR_GET_TASK_STS);
  JSON_STRING_TO_INT(RPCMGR_RPC_TASK_STS_ARGID, pPanelCmdObj->taskID);
  return 0;
}
int ADJsonRpcMgr::process_get_task_status(RPC_SRV_REQ *pReq) {
  const char *status_table[] = STR_RPC_SRV_RESULT_STRING_TABLE;
  RPCMGR_TASK_STS_PACKET *pPacket;
  pPacket = (RPCMGR_TASK_STS_PACKET *)pReq->dataRef;
  int taskSts;
  char errMsg[255];
  pReq->result =
      AsyncTaskWorker.get_task_status(pPacket->taskID, &taskSts, errMsg);
  strcpy(pPacket->task_sts, status_table[taskSts]);
  return 0;
}
int ADJsonRpcMgr::bin_to_json_get_task_sts(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_STRING(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET,
                           RPCMGR_RPC_TASK_STS_ARGSTS, task_sts);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_get_rpc_srv_version(JsonDataCommObj *pReq) {
  RPCMGR_INTEGER_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_INTEGER_PACKET, RPC_SRV_ACT_READ,
                       EJSON_RPCGMGR_GET_RPC_SRV_VERSION);
  return 0;
}
int ADJsonRpcMgr::process_rpc_server_version(RPC_SRV_REQ *pReq) {
  RPCMGR_INTEGER_PACKET *pPacket;
  pPacket = (RPCMGR_INTEGER_PACKET *)pReq->dataRef;
  if (pReq->action == RPC_SRV_ACT_READ) {
    pPacket->value = svnVersion;
    pReq->result = RPC_SRV_RESULT_SUCCESS;
  } else
    pReq->result = RPC_SRV_RESULT_ACTION_NOT_ALLOWED;
  return 0;
}
int ADJsonRpcMgr::bin_to_json_get_rpc_srv_version(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_INT(RPC_SRV_REQ, RPCMGR_INTEGER_PACKET,
                        RPCMGR_RPC_SER_VER_ARGVER, value);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_trigger_datasave(JsonDataCommObj *pReq) {
  RPCMGR_TASK_STS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCGMGR_TRIGGER_DATASAVE);
  return 0;
}
int ADJsonRpcMgr::process_trigger_datasave(RPC_SRV_REQ *pReq) {
  RPCMGR_TASK_STS_PACKET *pPacket;
  pPacket = (RPCMGR_TASK_STS_PACKET *)pReq->dataRef;
  RPCMGR_TASK_STS_PACKET *pWorkData = NULL;
  OBJECT_MEM_NEW(pWorkData, RPCMGR_TASK_STS_PACKET);
  if (AsyncTaskWorker.push_task(EJSON_RPCGMGR_TRIGGER_DATASAVE,
                                (unsigned char *)pWorkData, &pPacket->taskID,
                                WORK_CMD_AFTER_DONE_PRESERVE) == 0)
    pReq->result = RPC_SRV_RESULT_IN_PROG;
  else {
    OBJ_MEM_DELETE(pWorkData);
    pReq->result = RPC_SRV_RESULT_FAIL;
  }
  return 0;
}
int ADJsonRpcMgr::bin_to_json_trigger_datasave(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_IN_PROG(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET,
                            RPCMGR_RPC_TASK_STS_ARGID);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_get_settings_sts(JsonDataCommObj *pReq) {
  RPCMGR_SETTINGS_STS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_SETTINGS_STS_PACKET,
                       RPC_SRV_ACT_READ, EJSON_RPCGMGR_GET_SETTINGS_STS);
  return 0;
}
int ADJsonRpcMgr::process_get_settings_status(RPC_SRV_REQ *pReq,
                                              JsonDataCommObj *pReqObj) {
  return ProcessWorkCmnRpc(pReqObj);
}
int ADJsonRpcMgr::bin_to_json_get_settings_sts(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_STRING(RPC_SRV_REQ, RPCMGR_SETTINGS_STS_PACKET,
                           RPCMGR_RPC_STTNG_STS_ARGSTS, status);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_shutdown_service(JsonDataCommObj *pReq) {
  RPCMGR_TASK_STS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCGMGR_SHUTDOWN_SERVICE);
  return 0;
}
int ADJsonRpcMgr::process_shutdown_service(RPC_SRV_REQ *pReq) {
  if (shutdown_support == false) {
    pReq->result = RPC_SRV_RESULT_FEATURE_UNSUPPORTED;
    return 0;
  }
  RPCMGR_TASK_STS_PACKET *pPacket;
  pPacket = (RPCMGR_TASK_STS_PACKET *)pReq->dataRef;
  if (pReq->action == RPC_SRV_ACT_WRITE) {
    RPCMGR_TASK_STS_PACKET *pWorkData = NULL;
    OBJECT_MEM_NEW(pWorkData, RPCMGR_TASK_STS_PACKET);
    if (AsyncTaskWorker.push_task(EJSON_RPCGMGR_SHUTDOWN_SERVICE,
                                  (unsigned char *)pWorkData, &pPacket->taskID,
                                  WORK_CMD_AFTER_DONE_DELETE) == 0)
      pReq->result = RPC_SRV_RESULT_IN_PROG;
    else {
      OBJ_MEM_DELETE(pWorkData);
      pReq->result = RPC_SRV_RESULT_FAIL;
    }
  } else {
    pReq->result = RPC_SRV_RESULT_ACTION_NOT_ALLOWED;
  }
  return 0;
}
int ADJsonRpcMgr::bin_to_json_shutdown_service(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_IN_PROG(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET,
                            RPCMGR_RPC_TRIG_SHUTDOWN_ARGID);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_reset_task_sts(JsonDataCommObj *pReq) {
  RPCMGR_TASK_STS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCGMGR_RESET_TASK_STS);
  return 0;
}
int ADJsonRpcMgr::process_reset_task_sts(RPC_SRV_REQ *pReq) {
  pReq->result = AsyncTaskWorker.reset_task_id_and_chain();
  return 0;
}
int ADJsonRpcMgr::bin_to_json_reset_task_sts(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_get_ready_sts(JsonDataCommObj *pReq) {
  RPCMGR_READY_STS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_READY_STS_PACKET, RPC_SRV_ACT_READ,
                       EJSON_RPCGMGR_GET_READY_STS);
  return 0;
}
int ADJsonRpcMgr::process_get_ready_status(RPC_SRV_REQ *pReq) {
  RPCMGR_READY_STS_PACKET *pPacket;
  pPacket = (RPCMGR_READY_STS_PACKET *)pReq->dataRef;
  pPacket->status = ServiceReadyFlag;
  pReq->result = RPC_SRV_RESULT_SUCCESS;
  return 0;
}
int ADJsonRpcMgr::bin_to_json_get_ready_sts(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_ENUM(
      RPC_SRV_REQ, RPCMGR_READY_STS_PACKET, RPCMGR_RPC_READY_STS_ARGSTS, status,
      RPCMGR_RPC_READY_STS_ARGSTS_TBL, EJSON_RPCGMGR_READY_STATE_UNKNOWN);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_get_debug_logging(JsonDataCommObj *pReq) {
  RPCMGR_DEBUG_LOG_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_DEBUG_LOG_PACKET, RPC_SRV_ACT_READ,
                       EJSON_RPCGMGR_GET_DEBUG_LOG);
  return 0;
}
int ADJsonRpcMgr::process_get_debug_logging(RPC_SRV_REQ *pReq) {
  RPCMGR_DEBUG_LOG_PACKET *pPacket;
  pPacket = (RPCMGR_DEBUG_LOG_PACKET *)pReq->dataRef;
  if (ServiceDebugFlag == true)
    pPacket->status = EJSON_RPCGMGR_FLAG_STATE_ENABLE;
  else if (ServiceDebugFlag == false)
    pPacket->status = EJSON_RPCGMGR_FLAG_STATE_DISABLE;
  else
    pPacket->status = EJSON_RPCGMGR_FLAG_STATE_UNKNOWN;
  pReq->result = RPC_SRV_RESULT_SUCCESS;
  return 0;
}
int ADJsonRpcMgr::bin_to_json_get_debug_logging(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_ENUM(
      RPC_SRV_REQ, RPCMGR_DEBUG_LOG_PACKET, RPCMGR_RPC_DEBUG_LOG_ARGSTS, status,
      RPCMGR_RPC_DEBUG_LOG_ARGSTS_TBL, EJSON_RPCGMGR_FLAG_STATE_UNKNOWN);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_set_debug_logging(JsonDataCommObj *pReq) {
  RPCMGR_DEBUG_LOG_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_DEBUG_LOG_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCGMGR_SET_DEBUG_LOG);
  JSON_STRING_TO_ENUM(RPCMGR_RPC_DEBUG_LOG_ARGSTS,
                      RPCMGR_RPC_DEBUG_LOG_ARGSTS_TBL, EJSON_RPCGMGR_FLAG_STATE,
                      EJSON_RPCGMGR_FLAG_STATE_UNKNOWN, pPanelCmdObj->status);
  return 0;
}
int ADJsonRpcMgr::process_set_debug_logging(RPC_SRV_REQ *pReq) {
  RPCMGR_DEBUG_LOG_PACKET *pPacket;
  pPacket = (RPCMGR_DEBUG_LOG_PACKET *)pReq->dataRef;
  switch (pPacket->status) {
  case EJSON_RPCGMGR_FLAG_STATE_DISABLE:
    ServiceDebugFlag = false;
    break;
  case EJSON_RPCGMGR_FLAG_STATE_ENABLE:
    ServiceDebugFlag = true;
    break;
  default:
    pReq->result = RPC_SRV_RESULT_ARG_ERROR;
    return 0;
  }
  setRpcDebugLogFlag(ServiceDebugFlag);
  pReq->result = RPC_SRV_RESULT_SUCCESS;
  return 0;
}
int ADJsonRpcMgr::bin_to_json_set_debug_logging(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, RPCMGR_DEBUG_LOG_PACKET);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_trigger_factory_store(JsonDataCommObj *pReq) {
  RPCMGR_TASK_STS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCGMGR_TRIGGER_FACTORY_STORE);
  return 0;
}
int ADJsonRpcMgr::process_trigger_factory_store(RPC_SRV_REQ *pReq) {
  RPCMGR_TASK_STS_PACKET *pPacket;
  pPacket = (RPCMGR_TASK_STS_PACKET *)pReq->dataRef;
  RPCMGR_TASK_STS_PACKET *pWorkData = NULL;
  OBJECT_MEM_NEW(pWorkData, RPCMGR_TASK_STS_PACKET);
  if (AsyncTaskWorker.push_task(EJSON_RPCGMGR_TRIGGER_FACTORY_STORE,
                                (unsigned char *)pWorkData, &pPacket->taskID,
                                WORK_CMD_AFTER_DONE_PRESERVE) == 0)
    pReq->result = RPC_SRV_RESULT_IN_PROG;
  else {
    OBJ_MEM_DELETE(pWorkData);
    pReq->result = RPC_SRV_RESULT_FAIL;
  }
  return 0;
}
int ADJsonRpcMgr::bin_to_json_trigger_factory_store(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_IN_PROG(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET,
                            RPCMGR_RPC_TASK_STS_ARGID);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_trigger_factory_restore(JsonDataCommObj *pReq) {
  RPCMGR_TASK_STS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCGMGR_TRIGGER_FACTORY_RESTORE);
  return 0;
}
int ADJsonRpcMgr::process_trigger_factory_restore(RPC_SRV_REQ *pReq) {
  RPCMGR_TASK_STS_PACKET *pPacket;
  pPacket = (RPCMGR_TASK_STS_PACKET *)pReq->dataRef;
  RPCMGR_TASK_STS_PACKET *pWorkData = NULL;
  OBJECT_MEM_NEW(pWorkData, RPCMGR_TASK_STS_PACKET);
  if (AsyncTaskWorker.push_task(EJSON_RPCGMGR_TRIGGER_FACTORY_RESTORE,
                                (unsigned char *)pWorkData, &pPacket->taskID,
                                WORK_CMD_AFTER_DONE_PRESERVE) == 0)
    pReq->result = RPC_SRV_RESULT_IN_PROG;
  else {
    OBJ_MEM_DELETE(pWorkData);
    pReq->result = RPC_SRV_RESULT_FAIL;
  }
  return 0;
}
int ADJsonRpcMgr::bin_to_json_trigger_factory_restore(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_IN_PROG(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET,
                            RPCMGR_RPC_TASK_STS_ARGID);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_event_subscribe(JsonDataCommObj *pReq) {
  char temp_param[255];
  RPCMGR_EVENT_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_EVENT_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCGMGR_EVENT_SUBSCRIBE);
  JSON_STRING_TO_INT(RPCMGR_RPC_EVENT_ARG_CLTTOK, pPanelCmdObj->cltToken);
  if (find_single_param((char *)pReq->socket_data,
                        (char *)RPCMGR_RPC_EVENT_ARG_PORT, temp_param) == 0) {
    JSON_STRING_TO_INT(RPCMGR_RPC_EVENT_ARG_PORT, pPanelCmdObj->portNum);
  } else
    pPanelCmdObj->portNum = -1;
  if (find_single_param((char *)pReq->socket_data,
                        (char *)RPCMGR_RPC_EVENT_ARG_EVENTNUM,
                        temp_param) == 0) {
    JSON_STRING_TO_INT(RPCMGR_RPC_EVENT_ARG_EVENTNUM, pPanelCmdObj->eventNum);
  } else
    pPanelCmdObj->eventNum = -1;
  pPanelCmdObj->sock_id = pReq->sock_id;
  pPanelCmdObj->sock_descr = pReq->sock_descr;
  strcpy(pPanelCmdObj->ip, pReq->ip);
  return 0;
}
int ADJsonRpcMgr::process_event_subscribe(RPC_SRV_REQ *pReq) {
  RPCMGR_EVENT_PACKET *pPacket;
  pPacket = (RPCMGR_EVENT_PACKET *)pReq->dataRef;
  EventEntry *pEvent = NULL;
  OBJECT_MEM_NEW(pEvent, EventEntry);
  if (pEvent == NULL)
    pReq->result = RPC_SRV_RESULT_FAIL;
  else {
    pEvent->cltToken = pPacket->cltToken;
    pEvent->portNum = pPacket->portNum;
    pEvent->eventNum = pPacket->eventNum;
    pEvent->sock_id = pPacket->sock_id;
    pEvent->sock_descr = pPacket->sock_descr;
    strcpy(pEvent->ip, pPacket->ip);
    pEvent->deleteFlag = false;
    int res = EventMgr.register_event_subscription(pEvent, &pPacket->srvToken);
    if (res == 0)
      pReq->result = RPC_SRV_RESULT_SUCCESS;
    else if (res == -1) {
      OBJ_MEM_DELETE(pEvent);
      pReq->result = RPC_SRV_RESULT_ITEM_DUPLICATE_FOUND;
    } else {
      OBJ_MEM_DELETE(pEvent);
      pReq->result = RPC_SRV_RESULT_FAIL;
    }
  }
  return 0;
}
int ADJsonRpcMgr::bin_to_json_event_subscribe(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_INT(RPC_SRV_REQ, RPCMGR_EVENT_PACKET,
                        RPCMGR_RPC_EVENT_ARG_SRVTOK, srvToken);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_event_unsubscribe(JsonDataCommObj *pReq) {
  RPCMGR_EVENT_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_EVENT_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCGMGR_EVENT_UNSUBSCRIBE);
  JSON_STRING_TO_INT(RPCMGR_RPC_EVENT_ARG_SRVTOK, pPanelCmdObj->srvToken);
  return 0;
}
int ADJsonRpcMgr::process_event_unsubscribe(RPC_SRV_REQ *pReq) {
  RPCMGR_EVENT_PACKET *pPacket;
  pPacket = (RPCMGR_EVENT_PACKET *)pReq->dataRef;
  int res = EventMgr.unregister_event_subscription(pPacket->srvToken);
  if (res == 0)
    pReq->result = RPC_SRV_RESULT_SUCCESS;
  else if (res == -1)
    pReq->result = RPC_SRV_RESULT_ITEM_NOT_FOUND;
  else
    pReq->result = RPC_SRV_RESULT_FAIL;
  return 0;
}
int ADJsonRpcMgr::bin_to_json_event_unsubscribe(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, RPCMGR_EVENT_PACKET);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_event_notify(JsonDataCommObj *pReq) {
  char temp_param[255];
  RPCMGR_EVENT_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_EVENT_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCGMGR_EVENT_NOTIFY);
  JSON_STRING_TO_INT(RPCMGR_RPC_EVENT_ARG_EVENTNUM, pPanelCmdObj->eventNum);
  if (find_single_param((char *)pReq->socket_data,
                        (char *)RPCMGR_RPC_EVENT_ARG_EXTRA, temp_param) == 0) {
    JSON_STRING_TO_INT(RPCMGR_RPC_EVENT_ARG_EXTRA, pPanelCmdObj->eventArg);
  } else
    pPanelCmdObj->eventArg = -1;
  if (find_single_param((char *)pReq->socket_data,
                        (char *)RPCMGR_RPC_EVENT_ARG2_EXTRA, temp_param) == 0) {
    JSON_STRING_TO_INT(RPCMGR_RPC_EVENT_ARG2_EXTRA, pPanelCmdObj->eventArg2);
  } else
    pPanelCmdObj->eventArg2 = -1;
  return 0;
}
int ADJsonRpcMgr::process_event_notify(RPC_SRV_REQ *pReq) {
  RPCMGR_EVENT_PACKET *pPacket;
  pPacket = (RPCMGR_EVENT_PACKET *)pReq->dataRef;
  int res = EventMgr.notify_event(pPacket->eventNum, pPacket->eventArg,
                                  pPacket->eventArg2);
  if (res == 0)
    pReq->result = RPC_SRV_RESULT_SUCCESS;
  else
    pReq->result = RPC_SRV_RESULT_FAIL;
  return 0;
}
int ADJsonRpcMgr::bin_to_json_event_notify(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, RPCMGR_EVENT_PACKET);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_event_process(JsonDataCommObj *pReq) {
  char temp_param[255];
  RPCMGR_EVENT_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_EVENT_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCGMGR_EVENT_PROCESS);
  JSON_STRING_TO_INT(RPCMGR_RPC_EVENT_ARG_CLTTOK, pPanelCmdObj->cltToken);
  JSON_STRING_TO_INT(RPCMGR_RPC_EVENT_ARG_EVENTNUM, pPanelCmdObj->eventNum);
  if (find_single_param((char *)pReq->socket_data,
                        (char *)RPCMGR_RPC_EVENT_ARG_EXTRA, temp_param) == 0) {
    JSON_STRING_TO_INT(RPCMGR_RPC_EVENT_ARG_EXTRA, pPanelCmdObj->eventArg);
  } else
    pPanelCmdObj->eventArg = -1;
  if (find_single_param((char *)pReq->socket_data,
                        (char *)RPCMGR_RPC_EVENT_ARG2_EXTRA, temp_param) == 0) {
    JSON_STRING_TO_INT(RPCMGR_RPC_EVENT_ARG2_EXTRA, pPanelCmdObj->eventArg2);
  } else
    pPanelCmdObj->eventArg2 = -1;
  return 0;
}
int ADJsonRpcMgr::process_event_process(RPC_SRV_REQ *pReq) {
  RPCMGR_EVENT_PACKET *pPacket;
  pPacket = (RPCMGR_EVENT_PACKET *)pReq->dataRef;
  EventMgr.process_event(pPacket->eventNum, pPacket->eventArg,
                         pPacket->cltToken, pPacket->eventArg2);
  pReq->result = RPC_SRV_RESULT_SUCCESS;
  return 0;
}
int ADJsonRpcMgr::bin_to_json_event_process(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, RPCMGR_EVENT_PACKET);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_trigger_run(JsonDataCommObj *pReq) {
  RPCMGR_TASK_STS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCGMGR_TRIGGER_RUN);
  return 0;
}
int ADJsonRpcMgr::process_trigger_run(RPC_SRV_REQ *pReq) {
  RPCMGR_TASK_STS_PACKET *pPacket;
  pPacket = (RPCMGR_TASK_STS_PACKET *)pReq->dataRef;
  RPCMGR_TASK_STS_PACKET *pWorkData = NULL;
  OBJECT_MEM_NEW(pWorkData, RPCMGR_TASK_STS_PACKET);
  if (AsyncTaskWorker.push_task(EJSON_RPCGMGR_TRIGGER_RUN,
                                (unsigned char *)pWorkData, &pPacket->taskID,
                                WORK_CMD_AFTER_DONE_PRESERVE) == 0)
    pReq->result = RPC_SRV_RESULT_IN_PROG;
  else {
    OBJ_MEM_DELETE(pWorkData);
    pReq->result = RPC_SRV_RESULT_FAIL;
  }
  return 0;
}
int ADJsonRpcMgr::bin_to_json_trigger_run(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_IN_PROG(RPC_SRV_REQ, RPCMGR_TASK_STS_PACKET,
                            RPCMGR_RPC_TASK_STS_ARGID);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_get_devop_state(JsonDataCommObj *pReq) {
  RPCMGR_DEVOP_STS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_DEVOP_STS_PACKET, RPC_SRV_ACT_READ,
                       EJSON_RPCMGR_GET_DEVOP_STATE);
  return 0;
}
int ADJsonRpcMgr::process_get_devop_state(RPC_SRV_REQ *pReq,
                                          JsonDataCommObj *pReqObj) {
  return ProcessWorkCmnRpc(pReqObj);
}
int ADJsonRpcMgr::bin_to_json_get_devop_state(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_ENUM(RPC_SRV_REQ, RPCMGR_DEVOP_STS_PACKET,
                         RPCMGR_RPC_DEVOP_STATE_ARGSTS, status,
                         RPCMGR_RPC_DEVOP_STATE_ARGSTS_TBL,
                         EJSON_RPCGMGR_DEVOP_STATE_UNKNOWN);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_set_devop_state(JsonDataCommObj *pReq) {
  RPCMGR_DEVOP_STS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_DEVOP_STS_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCMGR_SET_DEVOP_STATE);
  JSON_STRING_TO_ENUM(RPCMGR_RPC_DEVOP_STATE_ARGSTS,
                      RPCMGR_RPC_DEVOP_STATE_ARGSTS_TBL,
                      EJSON_RPCGMGR_DEVOP_STATE,
                      EJSON_RPCGMGR_DEVOP_STATE_UNKNOWN, pPanelCmdObj->status);
  return 0;
}
int ADJsonRpcMgr::process_set_devop_state(RPC_SRV_REQ *pReq) {
  RPCMGR_TASK_STS_PACKET *pPacket;
  pPacket = (RPCMGR_TASK_STS_PACKET *)pReq->dataRef;
  RPCMGR_TASK_STS_PACKET *pWorkData = NULL;
  OBJECT_MEM_NEW(pWorkData, RPCMGR_TASK_STS_PACKET);
  if (AsyncTaskWorker.push_task(EJSON_RPCMGR_SET_DEVOP_STATE,
                                (unsigned char *)pWorkData, &pPacket->taskID,
                                WORK_CMD_AFTER_DONE_PRESERVE) == 0)
    pReq->result = RPC_SRV_RESULT_IN_PROG;
  else {
    OBJ_MEM_DELETE(pWorkData);
    pReq->result = RPC_SRV_RESULT_FAIL;
  }
  return 0;
}
int ADJsonRpcMgr::bin_to_json_set_devop_state(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, RPCMGR_DEVOP_STS_PACKET);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_get_mw_byte(JsonDataCommObj *pReq) {
  RPCMGR_MIDDLEWARE_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET, RPC_SRV_ACT_READ,
                       EJSON_RPCMGR_GET_MW_BYTE);
  long unsigned int tmp;
  JSON_STRING_TO_ULONG(RPCMGR_RPC_MW_ARGADDR, tmp);
  pPanelCmdObj->addr = (uint32_t)tmp;
  return 0;
}
int ADJsonRpcMgr::process_get_mw_byte(RPC_SRV_REQ *pReq,
                                      JsonDataCommObj *pReqObj) {
  return ProcessWorkCmnRpc(pReqObj);
}
int ADJsonRpcMgr::bin_to_json_get_mw_byte(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_INT(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET,
                        RPCMGR_RPC_MW_ARGDATA, byte);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_set_mw_byte(JsonDataCommObj *pReq) {
  RPCMGR_MIDDLEWARE_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCMGR_SET_MW_BYTE);
  long unsigned int tmp;
  JSON_STRING_TO_ULONG(RPCMGR_RPC_MW_ARGADDR, tmp);
  pPanelCmdObj->addr = (uint32_t)tmp;
  JSON_STRING_TO_ULONG(RPCMGR_RPC_MW_ARGDATA, tmp);
  pPanelCmdObj->byte = (uint8_t)tmp;
  return 0;
}
int ADJsonRpcMgr::process_set_mw_byte(RPC_SRV_REQ *pReq,
                                      JsonDataCommObj *pReqObj) {
  return ProcessWorkCmnRpc(pReqObj);
}
int ADJsonRpcMgr::bin_to_json_set_mw_byte(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_get_mw_word(JsonDataCommObj *pReq) {
  RPCMGR_MIDDLEWARE_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET, RPC_SRV_ACT_READ,
                       EJSON_RPCMGR_GET_MW_WORD);
  long unsigned int tmp;
  JSON_STRING_TO_ULONG(RPCMGR_RPC_MW_ARGADDR, tmp);
  pPanelCmdObj->addr = (uint32_t)tmp;
  return 0;
}
int ADJsonRpcMgr::process_get_mw_word(RPC_SRV_REQ *pReq,
                                      JsonDataCommObj *pReqObj) {
  return ProcessWorkCmnRpc(pReqObj);
}
int ADJsonRpcMgr::bin_to_json_get_mw_word(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_INT(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET,
                        RPCMGR_RPC_MW_ARGDATA, word);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_set_mw_word(JsonDataCommObj *pReq) {
  RPCMGR_MIDDLEWARE_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCMGR_SET_MW_WORD);
  long unsigned int tmp;
  JSON_STRING_TO_ULONG(RPCMGR_RPC_MW_ARGADDR, tmp);
  pPanelCmdObj->addr = (uint32_t)tmp;
  JSON_STRING_TO_ULONG(RPCMGR_RPC_MW_ARGDATA, tmp);
  pPanelCmdObj->word = (uint8_t)tmp;
  return 0;
}
int ADJsonRpcMgr::process_set_mw_word(RPC_SRV_REQ *pReq,
                                      JsonDataCommObj *pReqObj) {
  return ProcessWorkCmnRpc(pReqObj);
}
int ADJsonRpcMgr::bin_to_json_set_mw_word(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_get_mw_dword(JsonDataCommObj *pReq) {
  RPCMGR_MIDDLEWARE_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET, RPC_SRV_ACT_READ,
                       EJSON_RPCMGR_GET_MW_DWORD);
  long unsigned int tmp;
  JSON_STRING_TO_ULONG(RPCMGR_RPC_MW_ARGADDR, tmp);
  pPanelCmdObj->addr = (uint32_t)tmp;
  return 0;
}
int ADJsonRpcMgr::process_get_mw_dword(RPC_SRV_REQ *pReq,
                                       JsonDataCommObj *pReqObj) {
  return ProcessWorkCmnRpc(pReqObj);
}
int ADJsonRpcMgr::bin_to_json_get_mw_dword(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_INT(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET,
                        RPCMGR_RPC_MW_ARGDATA, dword);
  return 0;
}
int ADJsonRpcMgr::json_to_bin_set_mw_dword(JsonDataCommObj *pReq) {
  RPCMGR_MIDDLEWARE_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_RPCMGR_SET_MW_DWORD);
  long unsigned int tmp;
  JSON_STRING_TO_ULONG(RPCMGR_RPC_MW_ARGADDR, tmp);
  pPanelCmdObj->addr = (uint32_t)tmp;
  JSON_STRING_TO_ULONG(RPCMGR_RPC_MW_ARGDATA, tmp);
  pPanelCmdObj->dword = (uint8_t)tmp;
  return 0;
}
int ADJsonRpcMgr::process_set_mw_dword(RPC_SRV_REQ *pReq,
                                       JsonDataCommObj *pReqObj) {
  return ProcessWorkCmnRpc(pReqObj);
}
int ADJsonRpcMgr::bin_to_json_set_mw_dword(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, RPCMGR_MIDDLEWARE_PACKET);
  return 0;
}
