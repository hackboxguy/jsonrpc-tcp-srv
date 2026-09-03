#!/bin/bash
# Concurrent-load scenario for xmproxysrv + sysmgr, optionally under a
# sanitizer build. Starts the rig and daemons from INSTALL, then runs N
# iterations of: chat regression + guest test + TCP JSON-RPC bursts against
# both daemons, all at once. Reports failures, daemon health and any
# sanitizer reports found in .run/.
#
#   stress-scenario.sh [--install DIR] [--iterations N] [--tcp-repeat N] [--keep]
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
INSTALL="$ROOT/Output/install"; ITER=2; REPEAT=300; KEEP=0
while [ $# -gt 0 ]; do case "$1" in
  --install) shift; INSTALL="$1";; --iterations) shift; ITER="$1";;
  --tcp-repeat) shift; REPEAT="$1";; --keep) KEEP=1;;
  *) echo "unknown option $1"; exit 2;; esac; shift; done
RUN="$HERE/.run"; VENV="$HERE/.venv"
source "$HERE/rig/accounts.env"; export XMPP_HOST XMPP_PORT XMPP_PORT2 XMPP_DOMAIN ADMIN_PW GUEST_PW
export LD_LIBRARY_PATH="$INSTALL/lib:${LD_LIBRARY_PATH:-}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}:log_path=$RUN/asan"
export TSAN_OPTIONS="${TSAN_OPTIONS:-}:log_path=$RUN/tsan"
FAILS=0
stop_daemons() { for p in xmproxysrv sysmgr; do [ -f "$RUN/$p.pid" ] && kill "$(cat "$RUN/$p.pid")" 2>/dev/null; done; sleep 1; }
cleanup() { if [ "$KEEP" = 1 ]; then echo "--keep: daemons and rig left running"; else stop_daemons; "$HERE/rig/rig.sh" down >/dev/null; fi; }
trap cleanup EXIT

"$HERE/rig/rig.sh" up || exit 1
stop_daemons; rm -rf "$RUN"; mkdir -p "$RUN"
cp "$HERE/fixtures/xmpp-alias-list.txt" "$HERE/fixtures/xmpp-botname.txt" "$RUN/"; : > "$RUN/xmpp-evntsubscr.txt"; : > "$RUN/xmpp-acl.txt"; cp "$HERE/fixtures/manifest.json" "$RUN/manifest.json"
SYSMGR_EMULATION_SHELL=1 "$INSTALL/bin/sysmgr" --syscfg=docker --emulation >"$RUN/sysmgr.log" 2>&1 & echo $! > "$RUN/sysmgr.pid"; sleep 1
"$INSTALL/bin/xmproxysrv" --syscfg=docker --port=40005 --loginfile="$HERE/fixtures/xmpp-login.txt" \
  --aliaslist="$RUN/xmpp-alias-list.txt" --botname="$RUN/xmpp-botname.txt" \
  --evntsubscr="$RUN/xmpp-evntsubscr.txt" --aclfile="$RUN/xmpp-acl.txt" --manifest="$RUN/manifest.json" --subscrfile="$RUN/xmpp-subscriptions.txt" --iface=lo >"$RUN/xmproxysrv.log" 2>&1 & echo $! > "$RUN/xmproxysrv.pid"
for i in $(seq 1 30); do
  "$VENV/bin/python" - <<PY 2>/dev/null && break
import socket,sys
s=socket.create_connection(("127.0.0.1",40005),2); s.sendall(b'{ "jsonrpc": "2.0", "method": "get_online_status", "id": 0 }\n')
sys.exit(0 if b'"online"' in s.recv(4096) else 1)
PY
  sleep 1; [ "$i" = 30 ] && { echo "bot did not come online"; exit 1; }
done
echo "bot online; running $ITER iteration(s), tcp repeat $REPEAT, install $INSTALL"
for i in $(seq 1 "$ITER"); do
  PIDS=()
  timeout 240 "$VENV/bin/python" "$HERE/test_chat_regression.py" >"$RUN/chat$i.log" 2>&1 & PIDS+=($!)
  timeout 120 "$INSTALL/bin/tcp-json-rpc-client" --servertcpport=40005 --requests="$ROOT/utils/tests/xmproxy-test/requests.txt" --responses="$ROOT/utils/tests/xmproxy-test/responses.txt" --repeat="$REPEAT" >"$RUN/tcp$i.log" 2>&1 & PIDS+=($!)
  timeout 120 "$INSTALL/bin/tcp-json-rpc-client" --servertcpport=40001 --requests="$ROOT/utils/tests/sysmgr-test/requests.txt" --responses="$ROOT/utils/tests/sysmgr-test/responses.txt" --repeat="$REPEAT" >"$RUN/tcpsys$i.log" 2>&1 & PIDS+=($!)
  wait "${PIDS[@]}"
  C=$(grep -c '^FAIL' "$RUN/chat$i.log")
  T=$(grep -E 'mismatch' "$RUN/tcp$i.log" | tr -d '\n'); TS=$(grep -E 'mismatch' "$RUN/tcpsys$i.log" | tr -d '\n')
  echo "iteration $i: chat fails=$C xmproxy-tcp [$T] sysmgr-tcp [$TS]"
  [ "$C" = 0 ] && [ "$T" = "mismatch: 0" ] && [ "$TS" = "mismatch: 0" ] || FAILS=$((FAILS+1))
  for p in sysmgr xmproxysrv; do kill -0 "$(cat "$RUN/$p.pid")" 2>/dev/null || { echo "  $p DIED"; FAILS=$((FAILS+1)); }; done
done
# guest access once at the end: it changes roster state that the chat
# regression's buddy-list cases would otherwise see
timeout 120 "$VENV/bin/python" "$HERE/test_guest_access.py" >"$RUN/guest.log" 2>&1
G=$(grep -c '^FAIL' "$RUN/guest.log"); echo "guest access: fails=$G"; [ "$G" = 0 ] || FAILS=$((FAILS+1))
for p in sysmgr xmproxysrv; do kill -0 "$(cat "$RUN/$p.pid")" 2>/dev/null || { echo "  $p DIED"; FAILS=$((FAILS+1)); }; done
for f in "$RUN"/asan.* "$RUN"/tsan.*; do [ -f "$f" ] || continue; echo "=== sanitizer report $f"; grep -E "ERROR|SUMMARY|WARNING: ThreadSanitizer|#[0-3] " "$f" | head -24; FAILS=$((FAILS+1)); done
echo; [ "$FAILS" = 0 ] && echo "STRESS RESULT: passed" || echo "STRESS RESULT: $FAILS problem(s)"
exit $FAILS
