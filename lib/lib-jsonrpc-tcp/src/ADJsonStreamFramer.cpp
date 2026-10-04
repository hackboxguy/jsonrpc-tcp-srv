#include "ADJsonStreamFramer.hpp"
ADJsonStreamFramer::ADJsonStreamFramer(size_t max_msg_size)
    : max_msg(max_msg_size), scan(0), start(0), depth(0), in_string(false),
      escape(false), error(false) {}
void ADJsonStreamFramer::reset() {
  buf.clear();
  ready.clear();
  scan = 0;
  start = 0;
  depth = 0;
  in_string = false;
  escape = false;
  error = false;
}
size_t ADJsonStreamFramer::pending_bytes() const { return buf.size(); }
bool ADJsonStreamFramer::next(std::string &out) {
  if (ready.empty())
    return false;
  out.swap(ready.front());
  ready.pop_front();
  return true;
}
int ADJsonStreamFramer::feed(const char *data, size_t len) {
  if (error)
    return -1;
  if (data != NULL && len > 0)
    buf.append(data, len);
  size_t consumed = 0; // bytes of buf that are no longer needed
  for (; scan < buf.size(); scan++) {
    char c = buf[scan];
    if (depth == 0) {
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0' ||
          c == ',') {
        consumed = scan + 1;
        continue;
      }
      if (c != '{' && c != '[') {
        error = true;
        break;
      }
      start = scan;
      depth = 1;
      in_string = false;
      escape = false;
      continue;
    }
    if (scan - start + 1 > max_msg) {
      error = true;
      break;
    }
    if (in_string) {
      if (escape)
        escape = false;
      else if (c == '\\')
        escape = true;
      else if (c == '"')
        in_string = false;
    } else if (c == '"') {
      in_string = true;
    } else if (c == '{' || c == '[') {
      depth++;
    } else if (c == '}' || c == ']') {
      if (--depth == 0) {
        ready.push_back(buf.substr(start, scan - start + 1));
        consumed = scan + 1;
      }
    }
  }
  if (error) {
    buf.clear();
    scan = 0;
    start = 0;
    depth = 0;
    return -1;
  }
  // compact once per feed() instead of per byte
  if (depth > 0 && start > consumed)
    consumed = start;
  if (consumed > 0) {
    buf.erase(0, consumed);
    scan -= consumed;
    if (depth > 0)
      start -= consumed;
  }
  return (int)ready.size();
}
