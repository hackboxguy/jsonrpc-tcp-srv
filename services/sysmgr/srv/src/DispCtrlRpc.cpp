#include "DispCtrlRpc.h"
#include "ADSysInfo.hpp"
#include "DisplayDevice.hpp"
/* ------------------------------------------------------------------------- */
DispCtrlRpc::DispCtrlRpc(std::string rpcName, int myIndex, bool emu, bool log,
                         SYSMGR_CMN_DATA_CACHE *pData)
    : ADJsonRpcMgrConsumer(rpcName, myIndex, emu, log) {
  pDataCache = pData;
}
/* ------------------------------------------------------------------------- */
DispCtrlRpc::~DispCtrlRpc() {}
/* ------------------------------------------------------------------------- */
int DispCtrlRpc::MapJsonToBinary(JsonDataCommObj *pReq, int index) {
  EJSON_SYSMGR_RPC_TYPES command = (EJSON_SYSMGR_RPC_TYPES)index;
  switch (command) {
  case EJSON_SYSMGR_RPC_DISP_CLEAR:
    return json_to_bin_disp_clear(pReq);
  case EJSON_SYSMGR_RPC_DISP_PRINT:
    return json_to_bin_disp_print(pReq);
  case EJSON_SYSMGR_RPC_DISP_GET_BKLT:
    return json_to_bin_backlight_get(pReq);
  case EJSON_SYSMGR_RPC_DISP_SET_BKLT:
    return json_to_bin_backlight_set(pReq);
  default:
    return -1;
  }
  return -1;
}
/* ------------------------------------------------------------------------- */
int DispCtrlRpc::MapBinaryToJson(JsonDataCommObj *pReq, int index) {
  EJSON_SYSMGR_RPC_TYPES command = (EJSON_SYSMGR_RPC_TYPES)index;
  switch (command) {
  case EJSON_SYSMGR_RPC_DISP_CLEAR:
    return bin_to_json_disp_clear(pReq);
  case EJSON_SYSMGR_RPC_DISP_PRINT:
    return bin_to_json_disp_print(pReq);
  case EJSON_SYSMGR_RPC_DISP_GET_BKLT:
    return bin_to_json_backlight_get(pReq);
  case EJSON_SYSMGR_RPC_DISP_SET_BKLT:
    return bin_to_json_backlight_set(pReq);
  default:
    return -1;
  }
  return -1;
}
/* ------------------------------------------------------------------------- */
int DispCtrlRpc::ProcessWork(JsonDataCommObj *pReq, int index,
                             ADJsonRpcMgrProducer *pObj) {
  EJSON_SYSMGR_RPC_TYPES command = (EJSON_SYSMGR_RPC_TYPES)index;
  switch (command) {
  case EJSON_SYSMGR_RPC_DISP_CLEAR:
    return process_disp_clear(pReq, pDataCache);
  case EJSON_SYSMGR_RPC_DISP_PRINT:
    return process_disp_print(pReq, pDataCache);
  case EJSON_SYSMGR_RPC_DISP_GET_BKLT:
    return process_backlight_get(pReq, pDataCache);
  case EJSON_SYSMGR_RPC_DISP_SET_BKLT:
    return process_backlight_set(pReq, pDataCache);
  default:
    return -1;
  }
  return 0;
}
/* ------------------------------------------------------------------------- */
RPC_SRV_RESULT DispCtrlRpc::ProcessWorkAsync(int index,
                                             unsigned char *pWorkData) {
  RPC_SRV_RESULT ret_val = RPC_SRV_RESULT_FAIL;

  return ret_val;
}
/* ------------------------------------------------------------------------- */
// EJSON_SYSMGR_RPC_DISP_CLEAR
int DispCtrlRpc::json_to_bin_disp_clear(JsonDataCommObj *pReq) {
  SYSMGR_PRINT_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, SYSMGR_PRINT_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_SYSMGR_RPC_DISP_CLEAR);
  return 0;
}
int DispCtrlRpc::bin_to_json_disp_clear(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, SYSMGR_PRINT_PACKET);
  return 0;
}
int DispCtrlRpc::process_disp_clear(JsonDataCommObj *pReq,
                                    SYSMGR_CMN_DATA_CACHE *pData) {
  DisplayDevice *pDisp = (DisplayDevice *)pData->pDisplay;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  SYSMGR_PRINT_PACKET *pPacket;
  pPacket = (SYSMGR_PRINT_PACKET *)pPanelReq->dataRef;
  if (pData->pDisplay != NULL)
    pPanelReq->result = pDisp->clear_display();
  else
    pPanelReq->result = RPC_SRV_RESULT_FAIL;
  return 0;
}
/* ------------------------------------------------------------------------- */
// EJSON_SYSMGR_RPC_DISP_PRINT
int DispCtrlRpc::json_to_bin_disp_print(JsonDataCommObj *pReq) {
  SYSMGR_PRINT_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, SYSMGR_PRINT_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_SYSMGR_RPC_DISP_PRINT);
  // after assigning pointer to pPanelReq->dataRef, modify cmd-req-obj with
  // correct client arguments
  JSON_STRING_TO_ENUM(SYSMGR_RPC_DISP_PRINT_LINE_ARG,
                      SYSMGR_RPC_DISP_LINE_ARG_TABL, EJSON_SYSMGR_LINE,
                      EJSON_SYSMGR_LINE_UNKNOWN, pPanelCmdObj->line);
  // extract targetFilePath
  JSON_STRING_TO_STRING(SYSMGR_RPC_DISP_PRINT_MESG_ARG, pPanelCmdObj->msg);
  return 0;
}
int DispCtrlRpc::bin_to_json_disp_print(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, SYSMGR_PRINT_PACKET);
  return 0;
}
int DispCtrlRpc::process_disp_print(JsonDataCommObj *pReq,
                                    SYSMGR_CMN_DATA_CACHE *pData) {
  DisplayDevice *pDisp = (DisplayDevice *)pData->pDisplay;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  SYSMGR_PRINT_PACKET *pPacket;
  pPacket = (SYSMGR_PRINT_PACKET *)pPanelReq->dataRef;
  DISPLAY_LINE myline =
      (DISPLAY_LINE)pPacket->line; // important: keep enum DISPLAY_LINE and
                                   // EJSON_DISPSRV_LINE same
  if (pData->pDisplay != NULL)
    pPanelReq->result = pDisp->print_line(pPacket->msg, myline);
  else
    pPanelReq->result = RPC_SRV_RESULT_FAIL;
  return 0;
}
/* ------------------------------------------------------------------------- */
int DispCtrlRpc::json_to_bin_backlight_get(JsonDataCommObj *pReq) {
  SYSMGR_PRINT_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, SYSMGR_PRINT_PACKET, RPC_SRV_ACT_READ,
                       EJSON_SYSMGR_RPC_DISP_GET_BKLT);
  return 0;
}
int DispCtrlRpc::bin_to_json_backlight_get(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP_ENUM(
      RPC_SRV_REQ, SYSMGR_PRINT_PACKET, SYSMGR_RPC_DISP_BKLT_ARG, bklsts,
      SYSMGR_RPC_DISP_BKLT_ARG_TABL, SYSMGR_BKLT_STS_UNKNOWN);
  return 0;
}
int DispCtrlRpc::process_backlight_get(JsonDataCommObj *pReq,
                                       SYSMGR_CMN_DATA_CACHE *pData) {
  bool sts;
  DisplayDevice *pDisp = (DisplayDevice *)pData->pDisplay;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  SYSMGR_PRINT_PACKET *pPacket;
  pPacket = (SYSMGR_PRINT_PACKET *)pPanelReq->dataRef;
  if (pData->pDisplay != NULL) {
    pPanelReq->result = pDisp->get_back_light(sts);
    if (sts == true)
      pPacket->bklsts = SYSMGR_BKLT_STS_ON;
    else
      pPacket->bklsts = SYSMGR_BKLT_STS_OFF;
  } else
    pPanelReq->result = RPC_SRV_RESULT_FAIL;
  return 0;
}
/* ------------------------------------------------------------------------- */
int DispCtrlRpc::json_to_bin_backlight_set(JsonDataCommObj *pReq) {
  SYSMGR_PRINT_PACKET *pPanelCmdObj = NULL;
  PREPARE_JSON_REQUEST(RPC_SRV_REQ, SYSMGR_PRINT_PACKET, RPC_SRV_ACT_WRITE,
                       EJSON_SYSMGR_RPC_DISP_SET_BKLT);
  JSON_STRING_TO_ENUM(SYSMGR_RPC_DISP_BKLT_ARG, SYSMGR_RPC_DISP_BKLT_ARG_TABL,
                      SYSMGR_BKLT_STS, SYSMGR_BKLT_STS_UNKNOWN,
                      pPanelCmdObj->bklsts);
  return 0;
}
int DispCtrlRpc::bin_to_json_backlight_set(JsonDataCommObj *pReq) {
  PREPARE_JSON_RESP(RPC_SRV_REQ, SYSMGR_PRINT_PACKET);
  return 0;
}
int DispCtrlRpc::process_backlight_set(JsonDataCommObj *pReq,
                                       SYSMGR_CMN_DATA_CACHE *pData) {
  DisplayDevice *pDisp = (DisplayDevice *)pData->pDisplay;
  RPC_SRV_REQ *pPanelReq = NULL;
  pPanelReq = (RPC_SRV_REQ *)pReq->pDataObj;
  SYSMGR_PRINT_PACKET *pPacket;
  pPacket = (SYSMGR_PRINT_PACKET *)pPanelReq->dataRef;
  if (pData->pDisplay != NULL) {
    if (pPacket->bklsts == SYSMGR_BKLT_STS_ON)
      pPanelReq->result = pDisp->set_back_light(true);
    else
      pPanelReq->result = pDisp->set_back_light(false);
  } else
    pPanelReq->result = RPC_SRV_RESULT_FAIL;
  return 0;
}
/* ------------------------------------------------------------------------- */
