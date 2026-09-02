#!/bin/bash
# One-command test run for xmproxysrv against the local rig.
#
#   run-tests.sh [--no-build] [--keep] [--record] [--only NAME] [--quick] [--install DIR]
#
# --install DIR runs the daemons from another install tree (for example a
# sanitizer build) and implies --no-build.
#
# Steps: build (unless --no-build), start Prosody rig, start sysmgr and
# xmproxysrv with test fixtures, wait until the bot is online, run
#   1. chat golden regression   (test_chat_regression.py)
#   2. guest access baseline    (test_guest_access.py)
#   3. TCP JSON-RPC regression  (tcp-json-rpc-client with utils/tests/xmproxy-test)
#   3b. roles and ACL enforcement (test_acl.py)
#   4. stress: two senders while the XMPP server restarts (test_stress.py)
#   5. failover to the fallback account and back (test_failover.py)
#   6. resilience: server restart, peer restart, SIGTERM (test_resilience.py)
# then stop everything (unless --keep). --quick skips groups 4 to 6.
# Exit code is non-zero on any failure.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
OUT="$ROOT/Output"
INSTALL="$OUT/install"
RUN="$HERE/.run"
VENV="$HERE/.venv"
BUILD=1; KEEP=0; QUICK=0; EXTRA=()
while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) BUILD=0 ;;
        --keep) KEEP=1 ;;
        --quick) QUICK=1 ;;
        --install) shift; INSTALL="$1"; BUILD=0 ;;
        --record) EXTRA+=(--record) ;;
        --only) shift; EXTRA+=(--only "$1") ;;
        *) echo "unknown option $1"; exit 2 ;;
    esac; shift
done
source "$HERE/rig/accounts.env"
export XMPP_HOST XMPP_PORT XMPP_PORT2 XMPP_DOMAIN ADMIN_PW GUEST_PW
export LD_LIBRARY_PATH="$INSTALL/lib:${LD_LIBRARY_PATH:-}"
XMPROXY_PORT=40005
FAILS=0

say() { echo; echo "==== $*"; }
stop_daemons() {
    for p in xmproxysrv sysmgr; do
        if [ -f "$RUN/$p.pid" ]; then
            kill "$(cat "$RUN/$p.pid")" 2>/dev/null || true
            rm -f "$RUN/$p.pid"
        fi
    done
    sleep 1
}
cleanup() {
    if [ "$KEEP" = 1 ]; then
        echo; echo "--keep: rig and daemons left running (logs in $RUN, stop with: $HERE/rig/rig.sh down; kill \$(cat $RUN/*.pid))"
    else
        stop_daemons
        "$HERE/rig/rig.sh" down >/dev/null
    fi
}
trap cleanup EXIT

if [ "$BUILD" = 1 ]; then
    say "build"
    [ -d "$OUT" ] || cmake -H"$ROOT" -B"$OUT" -DCMAKE_INSTALL_PREFIX="$INSTALL" -DCMAKE_BUILD_TYPE=Release >"$ROOT/Output-configure.log" 2>&1 || { echo "configure failed, see Output-configure.log"; exit 1; }
    cmake --build "$OUT" -- install -j"$(nproc)" >"$ROOT/Output-build.log" 2>&1 || { echo "build failed, see Output-build.log"; tail -20 "$ROOT/Output-build.log"; exit 1; }
    echo "built $INSTALL/bin/xmproxysrv"
fi
[ -x "$VENV/bin/python" ] || { say "python venv"; python3 -m venv "$VENV" && "$VENV/bin/pip" -q install slixmpp || exit 1; }

say "rig"
"$HERE/rig/rig.sh" up || exit 1

say "daemons"
stop_daemons
rm -rf "$RUN"; mkdir -p "$RUN"
cp "$HERE/fixtures/xmpp-alias-list.txt" "$HERE/fixtures/xmpp-botname.txt" "$RUN/"
: > "$RUN/xmpp-evntsubscr.txt"; : > "$RUN/xmpp-acl.txt"
"$INSTALL/bin/sysmgr" --syscfg=docker --emulation >"$RUN/sysmgr.log" 2>&1 &
echo $! > "$RUN/sysmgr.pid"
sleep 1
"$INSTALL/bin/xmproxysrv" --debuglog --syscfg=docker --port=$XMPROXY_PORT \
    --loginfile="$HERE/fixtures/xmpp-login.txt" \
    --aliaslist="$RUN/xmpp-alias-list.txt" --botname="$RUN/xmpp-botname.txt" \
    --evntsubscr="$RUN/xmpp-evntsubscr.txt" --aclfile="$RUN/xmpp-acl.txt" --iface=lo >"$RUN/xmproxysrv.log" 2>&1 &
echo $! > "$RUN/xmproxysrv.pid"
for i in $(seq 1 30); do
    if "$VENV/bin/python" - <<PY 2>/dev/null; then break; fi
import socket,sys
s=socket.create_connection(("127.0.0.1",$XMPROXY_PORT),2)
s.sendall(b'{ "jsonrpc": "2.0", "method": "get_online_status", "id": 0 }\n')
sys.exit(0 if b'"online"' in s.recv(4096) else 1)
PY
    sleep 1
    [ "$i" = 30 ] && { echo "bot did not come online; see $RUN/xmproxysrv.log"; exit 1; }
done
echo "bot online after ${i}s"

say "1/3 chat golden regression"
"$VENV/bin/python" "$HERE/test_chat_regression.py" "${EXTRA[@]}" || FAILS=$((FAILS+1))

if [ ${#EXTRA[@]} -eq 0 ]; then
    say "2/3 guest access baseline"
    "$VENV/bin/python" "$HERE/test_guest_access.py" || FAILS=$((FAILS+1))

    say "3/3 TCP JSON-RPC regression"
    TCPOUT=$("$INSTALL/bin/tcp-json-rpc-client" --servertcpport=$XMPROXY_PORT \
        --requests="$ROOT/utils/tests/xmproxy-test/requests.txt" \
        --responses="$ROOT/utils/tests/xmproxy-test/responses.txt" --repeat=1 2>&1)
    echo "$TCPOUT"
    echo "$TCPOUT" | grep -q "mismatch: 0" && echo "$TCPOUT" | grep -q "received: 7" || FAILS=$((FAILS+1))

    say "3b roles and ACL enforcement"
    RUN_DIR="$RUN" timeout 300 "$VENV/bin/python" "$HERE/test_acl.py" || FAILS=$((FAILS+1))

    if [ "$QUICK" = 0 ]; then
        say "4/6 stress with server restarts"
        timeout 600 "$VENV/bin/python" "$HERE/test_stress.py" || FAILS=$((FAILS+1))

        say "5/6 failover to fallback account and back"
        RUN_DIR="$RUN" timeout 600 "$VENV/bin/python" "$HERE/test_failover.py" || FAILS=$((FAILS+1))

        say "6/6 resilience (server restart, peer restart, SIGTERM)"
        # runs last: it stops xmproxysrv with SIGTERM
        RUN_DIR="$RUN" SYSMGR_BIN="$INSTALL/bin/sysmgr" XMPROXY_PORT=$XMPROXY_PORT \
            timeout 300 "$VENV/bin/python" "$HERE/test_resilience.py" || FAILS=$((FAILS+1))
        RESILIENCE_RAN=1
    fi
fi

say "sanitizer reports"
if grep -l "Sanitizer\|runtime error" "$RUN"/sysmgr.log "$RUN"/xmproxysrv.log 2>/dev/null; then
    grep -h -A3 "ERROR: \|WARNING: ThreadSanitizer\|SUMMARY" "$RUN"/sysmgr.log "$RUN"/xmproxysrv.log | head -40; FAILS=$((FAILS+1))
else
    echo "none"
fi

say "daemon health"
for p in sysmgr xmproxysrv; do
    if [ "$p" = xmproxysrv ] && [ "${RESILIENCE_RAN:-0}" = 1 ]; then echo "xmproxysrv stopped by the resilience test (expected)"; continue; fi
    if kill -0 "$(cat "$RUN/$p.pid")" 2>/dev/null; then echo "$p alive"; else echo "$p DIED during tests (see $RUN/$p.log)"; FAILS=$((FAILS+1)); fi
done

echo
if [ "$FAILS" = 0 ]; then echo "RESULT: all test groups passed"; else echo "RESULT: $FAILS test group(s) failed"; fi
exit $FAILS
