#ifndef __JSON_CMNDEF_H_
#define __JSON_CMNDEF_H_
#include <json-c/json.h>
#define JSON_RPC_METHOD_NAME_MAX_LENGTH 256
#define JSON_RPC_METHOD_REQ_MAX_LENGTH 1100
#define JSON_RPC_METHOD_RESP_MAX_LENGTH 1100
typedef enum cmd_task_result_t {
  CMD_TASK_RESULT_FAIL = -1,
  CMD_TASK_RESULT_SUCCESS = 0,
  CMD_TASK_RESULT_INVALID_PARAM,
  CMD_TASK_RESULT_DEV_TIMEOUT,
  CMD_TASK_RESULT_DEV_ERR,
  CMD_TASK_RESULT_MEM_FAIL,
  CMD_TASK_RESULT_IN_PROGRESS,
  CMD_TASK_RESULT_NOT_STARTED,
  CMD_TASK_RESULT_ACTUAL_VALUE,
  CMD_TASK_RESULT_UNKNOWN
} CMD_TASK_RESULT;
#define MSG_CMD_TASK_RESULT_SUCCESS "Success"
#define MSG_CMD_TASK_RESULT_FAIL "Fail"
#define MSG_CMD_TASK_RESULT_IN_PROGRESS "InProgress"
#define MSG_CMD_TASK_RESULT_NOT_STARTED "NotStarted"
#define MSG_CMD_TASK_RESULT_DEV_TIMEOUT "Timeout"
#define MSG_CMD_TASK_RESULT_ARG_ERROR "ArgError"
#define MSG_CMD_TASK_RESULT_MEM_ERROR "MemError"
#define MSG_CMD_TASK_RESULT_BUS_ERROR "BusError"
#define MSG_CMD_TASK_RESULT_ACTION_BLOCKED "ActionBlocked"
#define MSG_CMD_TASK_RESULT_DEV_NO_ACCESS "DeviceError"
#define MSG_CMD_TASK_RESULT_UNKNOWN "Unknown"
#define MSG_CMD_TASK_RESULT_MEM_FAIL "MemError"
#define MSG_CMD_TASK_RESULT_INVALID_PARAM "ParamError"
#define MSG_CMD_TASK_RESULT_DEV_ERR "DevError"
#define MSG_CMD_TASK_RESULT_ACTUAL_VALUE "Actual"
typedef struct JsonDataCommObj_t {
  int req_id;
  int cmd_index;
  CMD_TASK_RESULT mapper_result;
  RPC_SRV_RESULT rpc_code;
  RPC_SRV_ACT rpc_action;
  int worker_result;
  char *socket_data;
  json_object *json_resp_obj;
  char *custom_result_string;
  int dataObjLen;
  unsigned char *pDataObj;
  int cltport;
  char ip[512];
  int sock_id;
  int sock_descr;
} JsonDataCommObj;
#define RPC_NAME_ARG_RESULT_OBJ "result"
#define RPC_NAME_ARG_RESULT_PARAM "return"
#define RPC_NAME_SERVER_GET_NAME "get_server_name"
#define RPC_NAME_SERVER_GET_API_VER "get_server_api_version"
#define RPC_NAME_SERVER_GET_VER "get_server_version"
#define RPC_NAME_SERVER_GET_RPC_LIST "get_server_api_list"
typedef enum rpc_action_t {
  ERPC_ACTION_READ = 0,
  ERPC_ACTION_WRITE,
  ERPC_ACTION_VERIFY,
  ERPC_ACTION_UNKNOWN,
  ERPC_ACTION_NONE
} ERPC_ACTION;
typedef struct rpc_comm_obj_t {
  ERPC_ACTION action;
  int cmd;
  int clientId;
  int reqIdent;
  CMD_TASK_RESULT result;
  unsigned int dataSize;
  unsigned char *dataRef;
} SRPC_COMM_OBJ;
#define RPC_COL_SENSOR0_RED "red_0"
#define RPC_COL_SENSOR1_RED "red_1"
#define RPC_COL_SENSOR0_GREEN "green_0"
#define RPC_COL_SENSOR1_GREEN "green_1"
#define RPC_COL_SENSOR0_BLUE "blue_0"
#define RPC_COL_SENSOR1_BLUE "blue_1"
#define RPC_COL_SENSOR0_WHITE "white_0"
#define RPC_COL_SENSOR1_WHITE "white_1"
#define RPC_ARG_COL_SEN0_RESULT_PARAM "cs_resp0"
#define RPC_ARG_COL_SEN1_RESULT_PARAM "cs_resp1"
#define RPC_ARG_COL_SENSOR_GAIN_PARAM "gain"
#define RPC_ARG_COL_SENSOR_PRESCALE_PARAM "prescale"
#define RPC_ARG_COL_SENSOR_PUL_PERIOD_PARAM "period"
#define RPC_ARG_COL_SENSOR_NUM_PULSES_PARAM "pulses"
#define RPC_BLK_LIGHT_MEAS_RED "blk_red"
#define RPC_BLK_LIGHT_MEAS_GREEN "blk_green"
#define RPC_BLK_LIGHT_MEAS_BLUE "blk_blue"
#define RPC_ARG_RED_X "red_X"
#define RPC_ARG_RED_Y "red_Y"
#define RPC_ARG_RED_Z "red_Z"
#define RPC_ARG_GREEN_X "green_X"
#define RPC_ARG_GREEN_Y "green_Y"
#define RPC_ARG_GREEN_Z "green_Z"
#define RPC_ARG_BLUE_X "blue_X"
#define RPC_ARG_BLUE_Y "blue_Y"
#define RPC_ARG_BLUE_Z "blue_Z"
#define RPC_ARG_X "X"
#define RPC_ARG_Y "Y"
#define RPC_ARG_Z "Z"
#define RPC_ARG_CALIB_TIME_STAMP_PARAM "timestamp"
#define RPC_ARG_CALIB_PROD_DATA_PARAM "proddata"
#define RPC_ARG_CALIB_PROC_VERSION_PARAM "processVer"
#define RPC_ARG_TARGET_COLOR_TEMP "colortemp"
#define RPC_ARG_TARGET_LOCUS_OFFSET "locusOffset"
#define RPC_ARG_TARGET_GAMMA_FACTOR "gammaFactor"
#define RPC_ARG_TARGET_BRIGHTNESS "brightness"
#define RPC_ARG_TARGET_x "target_x"
#define RPC_ARG_TARGET_y "target_y"
#define RPC_ARG_DELTA_X "delta_x"
#define RPC_ARG_DELTA_y "delta_y"
#define RPC_ARG_DELTA_Y "delta_Y"
#define RPC_ARG_COLOR_OUT_OF_TOLERANCE "colorOutOfTolerance"
#define RPC_ARG_COLOR_NOT_REACHABLE "colorNotReachable"
#define RPC_ARG_BRIGHTNESS_NOT_REACHABLE "brightnessNotReachable"
#define RPC_ARG_SPECTRAL_CALIB_MISSING "spectralCalibMissing"
#define RPC_ARG_SENSOR_STATUS "sensorState"
#endif
