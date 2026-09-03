#include "XmppRpc.h"
#include "XmppMgr.h"
/* ------------------------------------------------------------------------- */
XmppRpc::XmppRpc(std::string rpcName, int myIndex, bool emu, bool log,
                 XMPROXY_CMN_DATA_CACHE *pData)
    : ADJsonRpcMgrConsumer(rpcName, myIndex, emu, log) {
  pDataCache = pData;
}
/* ------------------------------------------------------------------------- */
XmppRpc::~XmppRpc() {}
/* ------------------------------------------------------------------------- */
int XmppRpc::MapJsonToBinary(JsonDataCommObj *pReq, int index) {
  EJSON_XMPROXY_RPC_TYPES command = (EJSON_XMPROXY_RPC_TYPES)index;
  switch (command) {
  case EJSON_XMPROXY_RPC_GET_ASYNCTASK:
    return json_to_bin_get_async_task_in_progress(pReq);
  case EJSON_XMPROXY_RPC_GET_ONLINE_STATUS:
    return json_to_bin_get_online_status(pReq);
  case EJSON_XMPROXY_RPC_SET_ONLINE_STATUS:
    return json_to_bin_set_online_status(pReq);
  case EJSON_XMPROXY_RPC_SET_SEND_MESSAGE:
    return json_to_bin_set_send_message(pReq);
  case EJSON_XMPROXY_RPC_SET_SUBSCRIBE:
    return json_to_bin_set_subscribe_message(pReq);
  case EJSON_XMPROXY_RPC_SET_ACCEPT_BUDDY:
    return json_to_bin_set_accept_buddy(pReq);
  case EJSON_XMPROXY_RPC_GET_INBOX_COUNT:
    return json_to_bin_get_inbox_count(pReq);
  case EJSON_XMPROXY_RPC_GET_INBOX:
    return json_to_bin_get_inbox_msg(pReq);
  case EJSON_XMPROXY_RPC_SET_INBOX_EMPTY:
    return json_to_bin_set_inbox_clean(pReq);
  default:
    break;
  }
  return -1; // 0;
}
/* ------------------------------------------------------------------------- */
int XmppRpc::MapBinaryToJson(JsonDataCommObj *pReq, int index) {
  EJSON_XMPROXY_RPC_TYPES command = (EJSON_XMPROXY_RPC_TYPES)index;
  switch (command) {
  case EJSON_XMPROXY_RPC_GET_ASYNCTASK:
    return bin_to_json_get_async_task_in_progress(pReq);
  case EJSON_XMPROXY_RPC_GET_ONLINE_STATUS:
    return bin_to_json_get_online_status(pReq);
  case EJSON_XMPROXY_RPC_SET_ONLINE_STATUS:
    return bin_to_json_set_online_status(pReq);
  case EJSON_XMPROXY_RPC_SET_SEND_MESSAGE:
    return bin_to_json_set_send_message(pReq);
  case EJSON_XMPROXY_RPC_SET_SUBSCRIBE:
    return bin_to_json_set_subscribe_message(pReq);
  case EJSON_XMPROXY_RPC_SET_ACCEPT_BUDDY:
    return bin_to_json_set_accept_buddy(pReq);
  case EJSON_XMPROXY_RPC_GET_INBOX_COUNT:
    return bin_to_json_get_inbox_count(pReq);
  case EJSON_XMPROXY_RPC_GET_INBOX:
    return bin_to_json_get_inbox_msg(pReq);
  case EJSON_XMPROXY_RPC_SET_INBOX_EMPTY:
    return bin_to_json_set_inbox_clean(pReq);

  default:
    break;
  }
  return -1; // 0;
}
/* ------------------------------------------------------------------------- */
int XmppRpc::ProcessWork(JsonDataCommObj *pReq, int index,
                         ADJsonRpcMgrProducer *pObj) {
  EJSON_XMPROXY_RPC_TYPES command = (EJSON_XMPROXY_RPC_TYPES)index;
  switch (command) {
  case EJSON_XMPROXY_RPC_GET_ASYNCTASK:
    return process_get_async_task_in_progress(pReq);
  case EJSON_XMPROXY_RPC_GET_ONLINE_STATUS:
    return process_get_online_status(pReq);
  case EJSON_XMPROXY_RPC_SET_ONLINE_STATUS:
    return process_set_online_status(pReq);
  case EJSON_XMPROXY_RPC_SET_SEND_MESSAGE:
    return process_set_send_message(pReq);
  case EJSON_XMPROXY_RPC_SET_SUBSCRIBE:
    return process_set_subscribe_message(pReq);
  case EJSON_XMPROXY_RPC_SET_ACCEPT_BUDDY:
    return process_set_accept_buddy(pReq);
  case EJSON_XMPROXY_RPC_GET_INBOX_COUNT:
    return process_get_inbox_count(pReq);
  case EJSON_XMPROXY_RPC_GET_INBOX:
    return process_get_inbox_msg(pReq);
  case EJSON_XMPROXY_RPC_SET_INBOX_EMPTY:
    return process_set_inbox_clean(pReq);
  default:
    break;
  }
  return 0;
}
/* ------------------------------------------------------------------------- */
XMPROXY_ASYNCTASK_TYPE XmppRpc::get_async_task_in_progress() {
  //!!!!!!!important: when a new async task is added to ProcessWorkAsync(),
  // ensure that it is also added in this function
  XMPROXY_ASYNCTASK_TYPE task = XMPROXY_ASYNCTASK_UNKNOWN;
  switch (pDataCache->AsyncCmdInProgress) {
  default:
    break;
  }
  return task;
}
RPC_SRV_RESULT XmppRpc::ProcessWorkAsync(int cmd, unsigned char *pWorkData) {
  //!!!!!!!important: when a new async task is added in this function,
  // ensure that it is also added in get_async_task_in_progress()
  RPC_SRV_RESULT ret_val = RPC_SRV_RESULT_FAIL;
  pDataCache->AsyncCmdInProgress = (EJSON_XMPROXY_RPC_TYPES)
      cmd; // To know which async command is in progress
  switch (cmd) {
  default:
    break;
  }
  pDataCache->AsyncCmdInProgress = EJSON_XMPROXY_RPC_NONE;
  return ret_val;
}
/* ------------------------------------------------------------------------- */
int XmppRpc::json_to_bin_get_async_task_in_progress(JsonDataCommObj *pReq) {
  XMPROXY_ASYNCTASK_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, XMPROXY_ASYNCTASK_PACKET, RPC_SRV_ACT_READ,
                       EJSON_XMPROXY_RPC_GET_ASYNCTASK);
  return 0;
}
int XmppRpc::bin_to_json_get_async_task_in_progress(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_ENUM(
      RPC_SRV_REQ, XMPROXY_ASYNCTASK_PACKET, XMPROXY_RPC_ASYNCTASK_ARG, task,
      XMPROXY_RPC_ASYNCTASK_ARG_TABL, XMPROXY_ASYNCTASK_UNKNOWN);
  return 0;
}
int XmppRpc::process_get_async_task_in_progress(JsonDataCommObj *pReq) {
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  XMPROXY_ASYNCTASK_PACKET *pPacket;
  pPacket = (XMPROXY_ASYNCTASK_PACKET *)pPanelReq->dataRef;
  if (pPanelReq->action != RPC_SRV_ACT_READ) {
    pPanelReq->result = RPC_SRV_RESULT_ACTION_NOT_ALLOWED;
    return 0;
  }
  pPacket->task = get_async_task_in_progress();
  pPanelReq->result = RPC_SRV_RESULT_SUCCESS;
  return 0;
}
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
int XmppRpc::json_to_bin_get_online_status(JsonDataCommObj *pReq) {
  XMPROXY_ONLINESTS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, XMPROXY_ONLINESTS_PACKET, RPC_SRV_ACT_READ,
                       EJSON_XMPROXY_RPC_GET_ONLINE_STATUS);
  return 0;
}
int XmppRpc::bin_to_json_get_online_status(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_ENUM(
      RPC_SRV_REQ, XMPROXY_ONLINESTS_PACKET, XMPROXY_RPC_ONLINE_STATUS_ARG,
      status, XMPROXY_RPC_ONLINE_STATUS_ARG_TABL, XMPROXY_ONLINESTS_UNKNOWN);
  return 0;
}
int XmppRpc::process_get_online_status(JsonDataCommObj *pReq) {
  XmppMgr *pMgr = (XmppMgr *)pDataCache->pXmpMgr;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  XMPROXY_ONLINESTS_PACKET *pPacket;
  pPacket = (XMPROXY_ONLINESTS_PACKET *)pPanelReq->dataRef;
  if (pPanelReq->action != RPC_SRV_ACT_READ) {
    pPanelReq->result = RPC_SRV_RESULT_ACTION_NOT_ALLOWED;
    return 0;
  }
  if (pMgr->get_connected_status() == true)
    pPacket->status = XMPROXY_ONLINESTS_ONLINE;
  else
    pPacket->status = XMPROXY_ONLINESTS_OFFLINE;
  pPanelReq->result = RPC_SRV_RESULT_SUCCESS;
  return 0;
}
int XmppRpc::json_to_bin_set_online_status(JsonDataCommObj *pReq) {
  XMPROXY_ONLINESTS_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, XMPROXY_ONLINESTS_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_XMPROXY_RPC_SET_ONLINE_STATUS);
  JSON_STRING_TO_ENUM(
      XMPROXY_RPC_ONLINE_STATUS_ARG, XMPROXY_RPC_ONLINE_STATUS_ARG_TABL,
      XMPROXY_ONLINESTS_TYPE, XMPROXY_ONLINESTS_UNKNOWN, pPanelCmdObj->status);
  return 0;
}
int XmppRpc::bin_to_json_set_online_status(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, XMPROXY_ONLINESTS_PACKET);
  return 0;
}
int XmppRpc::process_set_online_status(JsonDataCommObj *pReq) {
  XmppMgr *pMgr = (XmppMgr *)pDataCache->pXmpMgr;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  XMPROXY_ONLINESTS_PACKET *pPacket;
  pPacket = (XMPROXY_ONLINESTS_PACKET *)pPanelReq->dataRef;
  if (pPanelReq->action != RPC_SRV_ACT_WRITE) {
    pPanelReq->result = RPC_SRV_RESULT_ACTION_NOT_ALLOWED;
    return 0;
  }
  if (pPacket->status == XMPROXY_ONLINESTS_ONLINE)
    pPanelReq->result = pMgr->set_online_status(true);
  else
    pPanelReq->result = pMgr->set_online_status(false);
  return 0;
}
/* ------------------------------------------------------------------------- */
int XmppRpc::json_to_bin_set_send_message(JsonDataCommObj *pReq) {
  XMPROXY_SENDMSG_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, XMPROXY_SENDMSG_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_XMPROXY_RPC_SET_SEND_MESSAGE);
  JSON_STRING_TO_STRING(XMPROXY_RPC_SEND_MESSAGE_TO_ARG, pPanelCmdObj->to);
  JSON_STRING_TO_STRING(XMPROXY_RPC_SEND_MESSAGE_MSG_ARG, pPanelCmdObj->msg);
  return 0;
}
int XmppRpc::bin_to_json_set_send_message(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, XMPROXY_SENDMSG_PACKET);
  return 0;
}
int XmppRpc::process_set_send_message(JsonDataCommObj *pReq) {
  XmppMgr *pMgr = (XmppMgr *)pDataCache->pXmpMgr;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  XMPROXY_SENDMSG_PACKET *pPacket;
  pPacket = (XMPROXY_SENDMSG_PACKET *)pPanelReq->dataRef;
  // TODO: add subject line to the message
  std::string To(pPacket->to);
  std::string Msg(pPacket->msg);
  pPanelReq->result = pMgr->proc_cmd_send_message_internal(To, Msg);
  // response to this send message will be stored in Inbox
  return 0;
}
/* ------------------------------------------------------------------------- */
int XmppRpc::json_to_bin_set_subscribe_message(JsonDataCommObj *pReq) {
  XMPROXY_SUBSCRIBE_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, XMPROXY_SUBSCRIBE_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_XMPROXY_RPC_SET_SUBSCRIBE);
  JSON_STRING_TO_STRING(XMPROXY_RPC_SUBSCRIBE_TO_ARG, pPanelCmdObj->to);
  JSON_STRING_TO_STRING(XMPROXY_RPC_SUBSCRIBE_MSG_ARG, pPanelCmdObj->msg);
  return 0;
}
int XmppRpc::bin_to_json_set_subscribe_message(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, XMPROXY_SUBSCRIBE_PACKET);
  return 0;
}
int XmppRpc::process_set_subscribe_message(JsonDataCommObj *pReq) {
  XmppMgr *pMgr = (XmppMgr *)pDataCache->pXmpMgr;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  XMPROXY_SUBSCRIBE_PACKET *pPacket;
  pPacket = (XMPROXY_SUBSCRIBE_PACKET *)pPanelReq->dataRef;
  // pPacket->to;pPacket->msg;
  // TODO
  // cout<<"to:"<<pPacket->to<<":msg:"<<pPacket->msg<<endl;
  std::string To(pPacket->to);
  std::string Msg(pPacket->msg);
  pPanelReq->result = pMgr->proc_cmd_subscribe_message(To, Msg);
  // TODO: sleep for a while so that response arrives from other end
  // usleep(500000); // sleep 500ms
  // cout << "ResponseMsg: " << pMgr->ResponseMsg << endl;
  return 0;
}
/* ------------------------------------------------------------------------- */
int XmppRpc::json_to_bin_set_accept_buddy(JsonDataCommObj *pReq) {
  XMPROXY_ACCEPT_BUDDY_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, XMPROXY_ACCEPT_BUDDY_PACKET,
                       RPC_SRV_ACT_WRITE, EJSON_XMPROXY_RPC_SET_ACCEPT_BUDDY);
  JSON_STRING_TO_STRING(XMPROXY_RPC_ACCEPT_BUDDY_TO_ARG, pPanelCmdObj->to);
  JSON_STRING_TO_STRING(XMPROXY_RPC_ACCEPT_BUDDY_MSG_ARG, pPanelCmdObj->msg);
  return 0;
}
int XmppRpc::bin_to_json_set_accept_buddy(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, XMPROXY_ACCEPT_BUDDY_PACKET);
  return 0;
}
int XmppRpc::process_set_accept_buddy(JsonDataCommObj *pReq) {
  XmppMgr *pMgr = (XmppMgr *)pDataCache->pXmpMgr;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  XMPROXY_ACCEPT_BUDDY_PACKET *pPacket;
  pPacket = (XMPROXY_ACCEPT_BUDDY_PACKET *)pPanelReq->dataRef;
  std::string To(pPacket->to);
  std::string Msg(pPacket->msg);
  pPanelReq->result = pMgr->proc_cmd_add_buddy(To, Msg);
  // cout << "ResponseMsg: " << pMgr->ResponseMsg << endl;
  return 0;
}
/* ------------------------------------------------------------------------- */
int XmppRpc::json_to_bin_get_inbox_count(JsonDataCommObj *pReq) {
  XMPROXY_INBOX_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, XMPROXY_INBOX_PACKET, RPC_SRV_ACT_READ,
                       EJSON_XMPROXY_RPC_GET_INBOX_COUNT);
  return 0;
}
int XmppRpc::bin_to_json_get_inbox_count(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_INT(RPC_SRV_REQ, XMPROXY_INBOX_PACKET,
                        XMPROXY_RPC_INBOX_COUNT_ARG, inbox_count);
  return 0;
}
int XmppRpc::process_get_inbox_count(JsonDataCommObj *pReq) {
  XmppMgr *pMgr = (XmppMgr *)pDataCache->pXmpMgr;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  XMPROXY_INBOX_PACKET *pPacket;
  pPacket = (XMPROXY_INBOX_PACKET *)pPanelReq->dataRef;
  pPacket->inbox_count = 0;
  pPanelReq->result =
      pMgr->proc_cmd_get_inbox_count(pPacket->inbox_count); // by ref
  return 0;
}
/* ------------------------------------------------------------------------- */
int XmppRpc::json_to_bin_get_inbox_msg(JsonDataCommObj *pReq) {
  XMPROXY_INBOX_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, XMPROXY_INBOX_PACKET, RPC_SRV_ACT_READ,
                       EJSON_XMPROXY_RPC_GET_INBOX);
  JSON_STRING_TO_INT(XMPROXY_RPC_INBOX_INDEX_ARG, pPanelCmdObj->inbox_index);
  return 0;
}
int XmppRpc::bin_to_json_get_inbox_msg(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_STRING(RPC_SRV_REQ, XMPROXY_INBOX_PACKET,
                           XMPROXY_RPC_INBOX_MSG_ARG, message);
  return 0;
}
int XmppRpc::process_get_inbox_msg(JsonDataCommObj *pReq) {
  XmppMgr *pMgr = (XmppMgr *)pDataCache->pXmpMgr;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  XMPROXY_INBOX_PACKET *pPacket;
  pPacket = (XMPROXY_INBOX_PACKET *)pPanelReq->dataRef;
  std::string tmpMsg = "";
  pPanelReq->result =
      pMgr->proc_cmd_get_inbox_msg(pPacket->inbox_index, tmpMsg);
  strncpy(pPacket->message, tmpMsg.c_str(),
          sizeof(pPacket->message)); // limit message to 1kb
  return 0;
}
/* ------------------------------------------------------------------------- */
int XmppRpc::json_to_bin_set_inbox_clean(JsonDataCommObj *pReq) {
  XMPROXY_INBOX_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, XMPROXY_INBOX_PACKET, RPC_SRV_ACT_READ,
                       EJSON_XMPROXY_RPC_SET_INBOX_EMPTY);
  return 0;
}
int XmppRpc::bin_to_json_set_inbox_clean(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, XMPROXY_INBOX_PACKET);
  return 0;
}
int XmppRpc::process_set_inbox_clean(JsonDataCommObj *pReq) {
  XmppMgr *pMgr = (XmppMgr *)pDataCache->pXmpMgr;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  pPanelReq->result = pMgr->proc_cmd_get_inbox_empty();
  return 0;
}
/* ------------------------------------------------------------------------- */
