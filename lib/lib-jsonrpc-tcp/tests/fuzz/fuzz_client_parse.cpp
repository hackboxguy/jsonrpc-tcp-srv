// libFuzzer target: response parsing of ADJsonRpcClient and the server-side
// parameter extraction (find_single_param). Output buffers have the
// historical caller size of 255 bytes; ASan reports any overflow.
#include "ADJsonRpcClient.hpp"
#include <stdint.h>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static ADJsonRpcClient client; // no connection needed for parsing
  std::string json((const char *)data, size); // NUL terminated copy
  char *js = (char *)json.c_str();
  char r[255], v1[255], v2[255], v3[255], v4[255], v5[255];
  int i1, i2, i3, i4;
  client.find_json_result(js, (char *)"return", r);
  client.find_json_result_and_single_string_param(js, (char *)"return", r,
                                                  (char *)"a", v1);
  client.find_json_result_and_three_string_param(js, (char *)"return", r,
                                                 (char *)"a", v1, (char *)"b",
                                                 v2, (char *)"c", v3);
  client.find_json_result_and_four_int_param(
      js, (char *)"return", r, (char *)"a", &i1, (char *)"b", &i2,
      (char *)"c", &i3, (char *)"d", &i4);
  client.find_json_result_and_five_string_param_statusErrorInfo(
      js, (char *)"return", r, (char *)"a", v1, (char *)"b", v2, (char *)"c",
      v3, (char *)"d", v4, (char *)"e", v5);
  client.find_single_param(js, (char *)"a", v1, sizeof(v1));
  client.find_single_param(js, (char *)"b", v2); // deprecated 255 overload
  return 0;
}
