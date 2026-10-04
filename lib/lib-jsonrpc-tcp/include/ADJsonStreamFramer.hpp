#ifndef __ADJSON_STREAM_FRAMER_H_
#define __ADJSON_STREAM_FRAMER_H_
#include <deque>
#include <stddef.h>
#include <string>
// Splits a TCP byte stream into complete top-level JSON values (objects or
// arrays). TCP may deliver part of a message, several messages, or several
// messages plus the start of the next one in a single recv(); the scanner
// state is kept across feed() calls so every split point is handled.
// Rules:
// - between messages, whitespace, NUL bytes and ',' are skipped (old clients
//   send a trailing NUL after each request);
// - any other byte outside a message, a message that grows beyond max_msg,
//   or an unbalanced closing bracket is a protocol error. After an error
//   feed() returns -1 until reset() is called; messages completed before
//   the error can still be read with next();
// - braces/brackets inside JSON strings (including escaped quotes) are
//   ignored. Bracket types are not matched; the JSON parser rejects "{]".
// Not thread safe: use one framer per connection from one thread.
#define ADJSON_STREAM_FRAMER_DEFAULT_MAX_MSG (256 * 1024)
class ADJsonStreamFramer {
  std::string buf;
  std::deque<std::string> ready;
  size_t max_msg;
  size_t scan;  // next byte of buf to look at
  size_t start; // start of the current message (valid when depth > 0)
  int depth;
  bool in_string;
  bool escape;
  bool error;

public:
  explicit ADJsonStreamFramer(
      size_t max_msg_size = ADJSON_STREAM_FRAMER_DEFAULT_MAX_MSG);
  // appends raw bytes; returns the number of complete messages available
  // for next(), or -1 on a protocol error
  int feed(const char *data, size_t len);
  // pops one complete message
  bool next(std::string &out);
  // drops partial data, queued messages and the error state
  void reset();
  // bytes of an incomplete message that are buffered
  size_t pending_bytes() const;
  size_t available() const { return ready.size(); }
  bool has_error() const { return error; }
};
#endif
