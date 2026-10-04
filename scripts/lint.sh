#!/bin/sh
# Static analysis of lib/lib-jsonrpc-tcp: clang-tidy (config in .clang-tidy)
# and cppcheck. Needs compile_commands.json from a configured build dir.
# usage: scripts/lint.sh <build-dir> [-Werror]
# Analyzer findings (core/cplusplus/unix) outside the hardware drivers
# always fail; everything else is only reported unless -Werror is given
# (there is a backlog, mostly unchecked return values and the drivers).
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
  # must-stay-at-zero subset (review v3 V3-L6): analyzer findings for
  # memory, null and uninitialized-value bugs outside the hardware drivers
  fatal=$(grep -E "warning:.*\[clang-analyzer-(core|cplusplus|unix)" \
            "$BUILD/clang-tidy.log" |
          grep -vE "/src/(ss_oled|BitBang_I2C|I2C[A-Za-z0-9]*|ADFpgaMem|RTCDevice|DisplayDevice|LightSensor)\.cpp:" |
          sort -u)
  if [ -n "$fatal" ]; then
    echo "clang-tidy: findings in the fatal subset:"
    echo "$fatal"
    fatal_rc=1
  fi
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
[ -n "$fatal_rc" ] && exit 1
[ "$STRICT" = "-Werror" ] && exit $rc
exit 0
