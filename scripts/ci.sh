#!/bin/sh
# Local CI: configure, build and test one or more presets out of tree.
# usage: scripts/ci.sh [preset ...]   (default: default asan tsan)
# presets:
#   default  gcc, tests                    asan   ASan+UBSan, tests
#   tsan     ThreadSanitizer, tests        strict -Wall -Wextra ... -Werror
#   clang    clang, strict, tests          fuzz   clang libFuzzer smoke runs
#   lint     clang-tidy + cppcheck report  musl   Alpine container (docker)
# Set CI_SERVICES=OFF to skip the services (xmproxysrv needs gloox).
# Build dirs: $CI_OUT (default ./ci-out)/<preset>
set -e
SRC=$(cd "$(dirname "$0")/.." && pwd)
OUT=${CI_OUT:-$SRC/ci-out}
SERVICES=${CI_SERVICES:-ON}
JOBS=$(nproc 2>/dev/null || echo 2)
[ $# -eq 0 ] && set -- default asan tsan

run_preset() {
  preset=$1
  dir=$OUT/$preset
  cc=""
  opts="-DADLIB_BUILD_TESTS=ON -DADLIB_BUILD_SERVICES=$SERVICES"
  case $preset in
    default) ;;
    asan)    opts="$opts -DADLIB_SANITIZE=address" ;;
    tsan)    opts="$opts -DADLIB_SANITIZE=thread" ;;
    strict)  opts="$opts -DADLIB_STRICT_WARNINGS=ON" ;;
    clang)   cc=clang; opts="$opts -DADLIB_STRICT_WARNINGS=ON" ;;
    fuzz)    cc=clang; opts="$opts -DADLIB_FUZZ=ON" ;;
    lint)    cc=clang ;;
    musl)
      docker run --rm -v "$SRC":/src:ro -v "$OUT":/out -w /src alpine:3.19 sh -c \
        "apk add --no-cache cmake make g++ json-c-dev linux-headers \
           i2c-tools-dev >/dev/null && \
         CI_OUT=/out/musl-in CI_SERVICES=OFF sh /src/scripts/ci.sh default"
      return ;;
    *) echo "unknown preset $preset"; exit 2 ;;
  esac
  echo "=== $preset"
  if [ -n "$cc" ]; then
    CC=$cc CXX=${cc}++ cmake -S "$SRC" -B "$dir" $opts >/dev/null
  else
    cmake -S "$SRC" -B "$dir" $opts >/dev/null
  fi
  cmake --build "$dir" -j"$JOBS"
  if [ "$preset" = lint ]; then
    cmake --build "$dir" --target lint
  else
    ctest --test-dir "$dir" --output-on-failure -j"$JOBS"
  fi
}

for p in "$@"; do
  run_preset "$p"
done
