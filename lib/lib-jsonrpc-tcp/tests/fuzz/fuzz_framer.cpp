// libFuzzer target: ADJsonStreamFramer. The output for the input fed in one
// piece must equal the output for the same input fed in chunks whose sizes
// come from the input itself (differential check).
#include "ADJsonStreamFramer.hpp"
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 1)
    return 0;
  const uint8_t step_seed = data[0];
  const char *in = (const char *)data + 1;
  size_t len = size - 1;
  const size_t max_msg = 512;

  ADJsonStreamFramer whole(max_msg);
  bool whole_err = whole.feed(in, len) < 0;
  std::vector<std::string> a;
  std::string m;
  while (whole.next(m))
    a.push_back(m);

  ADJsonStreamFramer split(max_msg);
  bool split_err = false;
  std::vector<std::string> b;
  size_t pos = 0, i = 0;
  while (pos < len && !split_err) {
    size_t chunk = 1 + ((step_seed + i * 7) % 13);
    if (chunk > len - pos)
      chunk = len - pos;
    split_err = split.feed(in + pos, chunk) < 0;
    while (split.next(m))
      b.push_back(m);
    pos += chunk;
    i++;
  }
  if (whole_err != split_err)
    abort();
  if (!whole_err && (a != b || whole.pending_bytes() != split.pending_bytes()))
    abort();
  return 0;
}
