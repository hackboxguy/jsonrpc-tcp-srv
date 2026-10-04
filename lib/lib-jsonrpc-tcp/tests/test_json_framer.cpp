// ADJsonStreamFramer unit tests (review finding C8). Every corpus entry is
// fed in one piece, byte by byte, and at random split points; the output
// must be identical in all cases.
#include "ADJsonStreamFramer.hpp"
#include "adtest.hpp"
#include <stdlib.h>

namespace {
struct Result {
  std::vector<std::string> msgs;
  bool error;
  size_t pending;
};
Result feed_whole(const std::string &in, size_t max_msg = 1024) {
  ADJsonStreamFramer f(max_msg);
  Result r;
  r.error = f.feed(in.data(), in.size()) < 0;
  std::string m;
  while (f.next(m))
    r.msgs.push_back(m);
  r.pending = f.pending_bytes();
  return r;
}
// feeds the input in chunks; cuts are the chunk boundaries
Result feed_split(const std::string &in, const std::vector<size_t> &cuts,
                  size_t max_msg = 1024) {
  ADJsonStreamFramer f(max_msg);
  Result r;
  r.error = false;
  size_t pos = 0;
  std::string m;
  for (size_t i = 0; i <= cuts.size() && !r.error; i++) {
    size_t end = i < cuts.size() ? cuts[i] : in.size();
    if (f.feed(in.data() + pos, end - pos) < 0)
      r.error = true;
    while (f.next(m))
      r.msgs.push_back(m);
    pos = end;
  }
  while (f.next(m))
    r.msgs.push_back(m);
  r.pending = f.pending_bytes();
  return r;
}
Result feed_bytewise(const std::string &in, size_t max_msg = 1024) {
  std::vector<size_t> cuts;
  for (size_t i = 1; i < in.size(); i++)
    cuts.push_back(i);
  return feed_split(in, cuts, max_msg);
}
Result feed_random(const std::string &in, unsigned *seed,
                   size_t max_msg = 1024) {
  std::vector<size_t> cuts;
  size_t pos = 0;
  while (in.size() > 0) {
    pos += 1 + rand_r(seed) % (in.size() / 3 + 2);
    if (pos >= in.size())
      break;
    cuts.push_back(pos);
  }
  return feed_split(in, cuts, max_msg);
}
bool same(const Result &a, const Result &b) {
  if (a.error != b.error)
    return false;
  if (a.error) // after an error only the messages before it are defined
    return true;
  return a.msgs == b.msgs && a.pending == b.pending;
}
// runs one corpus entry in all feeding modes and returns the whole-feed
// result after checking all modes agree
Result check_all_modes(const std::string &in, size_t max_msg = 1024) {
  Result whole = feed_whole(in, max_msg);
  CHECK(same(whole, feed_bytewise(in, max_msg)));
  unsigned seed = 12345;
  for (int i = 0; i < 10000; i++) {
    Result r = feed_random(in, &seed, max_msg);
    if (!same(whole, r)) {
      printf("random split mismatch for input: %s\n", in.c_str());
      CHECK(false);
      break;
    }
  }
  return whole;
}
const std::string A = "{\"jsonrpc\":\"2.0\",\"method\":\"a\",\"id\":1}";
const std::string B = "{\"jsonrpc\":\"2.0\",\"method\":\"b\",\"id\":2}";
} // namespace

TEST_CASE("framer: single object") {
  Result r = check_all_modes(A);
  REQUIRE(r.msgs.size() == 1);
  CHECK(r.msgs[0] == A);
  CHECK(!r.error);
  CHECK_EQ(r.pending, (size_t)0);
}

TEST_CASE("framer: back to back objects without separator (A}{B)") {
  Result r = check_all_modes(A + B);
  REQUIRE(r.msgs.size() == 2);
  CHECK(r.msgs[0] == A);
  CHECK(r.msgs[1] == B);
}

TEST_CASE("framer: newline, CRLF, space and NUL separators") {
  const char *seps[] = {"\n", " \r\n ", "\t", ","};
  for (int i = 0; i < 4; i++) {
    Result r = check_all_modes(A + seps[i] + B + seps[i]);
    REQUIRE(r.msgs.size() == 2);
    CHECK(r.msgs[0] == A);
    CHECK(r.msgs[1] == B);
  }
  std::string nul_sep = A + std::string(1, '\0') + B + std::string(1, '\0');
  Result r = check_all_modes(nul_sep);
  REQUIRE(r.msgs.size() == 2);
  CHECK(r.msgs[1] == B);
}

TEST_CASE("framer: brackets and quotes inside strings") {
  const char *tricky[] = {
      "{\"params\":{\"msg\":\"a}{b\"},\"id\":1}",
      "{\"s\":\"{\"}",
      "{\"s\":\"}\"}",
      "{\"s\":\"\\\"}{\\\"\"}",
      "{\"s\":\"\\\\\"}",
      "{\"s\":\"\\\\\\\"}\"}",
      "{\"s\":\"\\u007d\\u007b[]\"}",
  };
  for (int i = 0; i < 7; i++) {
    std::string in = std::string(tricky[i]) + A;
    Result r = check_all_modes(in);
    REQUIRE(r.msgs.size() == 2);
    CHECK(r.msgs[0] == tricky[i]);
    CHECK(r.msgs[1] == A);
  }
}

TEST_CASE("framer: nested objects and arrays") {
  std::string n = "{\"p\":{\"a\":[1,[2,{\"b\":[]}],{}],\"c\":{\"d\":{}}}}";
  Result r = check_all_modes(n + n);
  REQUIRE(r.msgs.size() == 2);
  CHECK(r.msgs[0] == n);
}

TEST_CASE("framer: top-level array (batch) is one message") {
  std::string batch = "[" + A + "," + B + "]";
  Result r = check_all_modes(batch + A);
  REQUIRE(r.msgs.size() == 2);
  CHECK(r.msgs[0] == batch);
}

TEST_CASE("framer: leading whitespace") {
  Result r = check_all_modes("  \r\n\t" + A);
  REQUIRE(r.msgs.size() == 1);
  CHECK(r.msgs[0] == A);
}

TEST_CASE("framer: leading garbage is a protocol error") {
  Result r = check_all_modes("hello" + A);
  CHECK(r.error);
  CHECK(r.msgs.empty());
}

TEST_CASE("framer: garbage after a message keeps the message") {
  ADJsonStreamFramer f;
  std::string in = A + "xyz";
  CHECK_EQ(f.feed(in.data(), in.size()), -1);
  std::string m;
  CHECK(f.next(m));
  CHECK(m == A);
  CHECK(f.has_error());
  CHECK_EQ(f.feed("{}", 2), -1); // stays in error until reset
}

TEST_CASE("framer: unbalanced closing bracket is a protocol error") {
  CHECK(check_all_modes("}" + A).error);
  CHECK(check_all_modes(A + "]").error);
}

TEST_CASE("framer: message size limit") {
  size_t max_msg = 100;
  std::string head = "{\"p\":\"";
  std::string tail = "\"}";
  std::string exact =
      head + std::string(max_msg - head.size() - tail.size(), 'x') + tail;
  REQUIRE(exact.size() == max_msg);
  Result r = check_all_modes(exact, max_msg);
  CHECK(!r.error);
  REQUIRE(r.msgs.size() == 1);
  CHECK(r.msgs[0] == exact);
  std::string over =
      head + std::string(max_msg + 1 - head.size() - tail.size(), 'x') + tail;
  REQUIRE(over.size() == max_msg + 1);
  CHECK(check_all_modes(over, max_msg).error);
  // an incomplete message must not grow without bound either
  ADJsonStreamFramer f(max_msg);
  std::string open = "{\"p\":\"" + std::string(200, 'x');
  CHECK_EQ(f.feed(open.data(), open.size()), -1);
}

TEST_CASE("framer: incomplete trailing object stays pending") {
  std::string partial = "{\"jsonrpc\":\"2.0\",\"met";
  Result r = check_all_modes(A + partial);
  REQUIRE(r.msgs.size() == 1);
  CHECK_EQ(r.pending, partial.size());
}

TEST_CASE("framer: reset drops partial data and error state") {
  ADJsonStreamFramer f;
  f.feed("{\"a\":", 5);
  CHECK(f.pending_bytes() > 0);
  f.reset();
  CHECK_EQ(f.pending_bytes(), (size_t)0);
  CHECK_EQ(f.feed(A.data(), A.size()), 1);
  f.feed("x", 1);
  CHECK(f.has_error());
  f.reset();
  CHECK(!f.has_error());
  CHECK_EQ(f.available(), (size_t)0);
  CHECK_EQ(f.feed(B.data(), B.size()), 1);
  std::string m;
  CHECK(f.next(m));
  CHECK(m == B);
}

TEST_CASE("framer: many messages in one feed, buffer is compacted") {
  ADJsonStreamFramer f;
  std::string in;
  for (int i = 0; i < 1000; i++)
    in += A + "\n";
  CHECK_EQ(f.feed(in.data(), in.size()), 1000);
  CHECK_EQ(f.pending_bytes(), (size_t)0);
}

// differential fuzz: random bytes from a JSON-ish alphabet, random splits;
// the framer must never crash and must agree with the one-piece feed
TEST_CASE("framer: random input differential fuzz") {
  const char alphabet[] = "{}[]\"\\ ,:ab\n\0x";
  unsigned seed = 4711;
  for (int iter = 0; iter < 3000; iter++) {
    std::string in;
    int len = rand_r(&seed) % 64;
    for (int i = 0; i < len; i++)
      in.push_back(alphabet[rand_r(&seed) % (sizeof(alphabet) - 1)]);
    Result whole = feed_whole(in, 32);
    for (int k = 0; k < 20; k++) {
      Result r = feed_random(in, &seed, 32);
      if (!same(whole, r)) {
        CHECK(false);
        return;
      }
    }
    CHECK(same(whole, feed_bytewise(in, 32)));
  }
}

ADTEST_MAIN()
