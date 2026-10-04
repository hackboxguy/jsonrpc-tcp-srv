#!/bin/sh
# Static analysis of lib/lib-jsonrpc-tcp: clang-tidy (config in .clang-tidy)
# and cppcheck. Needs compile_commands.json from a configured build dir.
# usage: scripts/lint.sh <build-dir> [-Werror]
# Without -Werror findings are reported only (there is a backlog in the
# older hardware-driver sources).
BUILD=${1:?usage: lint.sh <build-dir> [-Werror]}
STRICT=$2
SRC=$(cd "$(dirname "$0")/.." && pwd)
rc=0
if command -v run-clang-tidy >/dev/null 2>&1; then
  run-clang-tidy -quiet -p "$BUILD" "$SRC/lib/lib-jsonrpc-tcp/src/.*\.cpp" \
    > "$BUILD/clang-tidy.log" 2>&1
  n=$(grep -c "warning:" "$BUILD/clang-tidy.log")
  echo "clang-tidy: $n warning(s), see $BUILD/clang-tidy.log"
  [ "$n" -gt 0 ] && rc=1
else
  echo "run-clang-tidy not found, skipped"
fi
if command -v cppcheck >/dev/null 2>&1; then
  cppcheck --quiet --enable=warning,portability,performance --inconclusive \
    --suppress=uninitMemberVar --suppress=missingIncludeSystem \
    --std=c++11 -I "$SRC/lib/lib-jsonrpc-tcp/include" \
    "$SRC/lib/lib-jsonrpc-tcp/src" > "$BUILD/cppcheck.log" 2>&1
  n=$(grep -c ": \(warning\|error\|performance\|portability\)" "$BUILD/cppcheck.log")
  echo "cppcheck: $n finding(s), see $BUILD/cppcheck.log"
  [ "$n" -gt 0 ] && rc=1
else
  echo "cppcheck not found, skipped"
fi
[ "$STRICT" = "-Werror" ] && exit $rc
exit 0
