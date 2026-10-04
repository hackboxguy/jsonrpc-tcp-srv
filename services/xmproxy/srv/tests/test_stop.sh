#!/bin/sh
# Service-level stop test (review v3 finding V3-H1): xmproxysrv must exit
# within 5 s of SIGTERM while its XMPP server accepts the connection but
# never answers (hung or filtered server, stalled handshake).
# usage: test_stop.sh <path-to-xmproxysrv>
# exit 0 pass, 1 fail, 77 skip (port 5222 busy or no python3)
BIN=$1
command -v python3 >/dev/null 2>&1 || exit 77
DIR=$(mktemp -d)
trap 'kill -9 $SRV $FAKE 2>/dev/null; rm -rf "$DIR"' EXIT

# fake XMPP server on 127.0.0.1:5222: accept, record it, stay silent
python3 - "$DIR/accepted" > "$DIR/fake.log" 2>&1 <<'PY' &
import socket, sys, time
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try:
    s.bind(("127.0.0.1", 5222))
except OSError:
    open(sys.argv[1] + ".busy", "w").close(); sys.exit(0)
s.listen(4)
conns = []
while True:
    c, _ = s.accept(); conns.append(c)
    open(sys.argv[1], "w").close()
PY
FAKE=$!
sleep 0.5
[ -e "$DIR/accepted.busy" ] && exit 77

cat > "$DIR/login.txt" <<EOT
user: test@127.0.0.1
pw: secret
adminbuddy: admin@127.0.0.1
EOT
PORT=$((23000 + $$ % 1000))
"$BIN" --loginfile="$DIR/login.txt" --port=$PORT \
  --aliaslist="$DIR/alias.txt" --botname="$DIR/bot.txt" \
  --evntsubscr="$DIR/evnt.txt" > "$DIR/srv.log" 2>&1 &
SRV=$!

# wait until the service is connected to the silent server
i=0
while [ ! -e "$DIR/accepted" ] && [ $i -lt 100 ]; do sleep 0.1; i=$((i+1)); done
[ -e "$DIR/accepted" ] || { echo "xmproxysrv never connected"; cat "$DIR/srv.log"; exit 1; }
sleep 1

kill -TERM $SRV
i=0
while kill -0 $SRV 2>/dev/null && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
if kill -0 $SRV 2>/dev/null; then
  echo "FAIL: xmproxysrv still running 5 s after SIGTERM"
  exit 1
fi
wait $SRV
echo "PASS: exited $((i * 100)) ms after SIGTERM (status $?)"
exit 0
