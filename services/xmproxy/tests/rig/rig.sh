#!/bin/bash
# Local XMPP test rig for xmproxy: two Prosody containers in Docker.
#   primary  127.0.0.1:5222  accounts bot, admin, guest  (@localhost)
#   fallback 127.0.0.1:5223  accounts bot2, admin, guest (@localhost)
# Both serve the domain "localhost"; clients pick a server by port.
# Usage: rig.sh up | down | status | stop-primary | start-primary | stop-fallback | start-fallback
# Passwords in accounts.env. Development only; never used on a device.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
NAME=xmproxy-test-prosody
NAME2=xmproxy-test-prosody2
IMAGE=prosody/prosody:latest
PORT=5222
PORT2=5223
source "$HERE/accounts.env"

start_one() { # name port accounts...
    local name="$1" port="$2"; shift 2
    if docker ps --format '{{.Names}}' | grep -qx "$name"; then
        echo "rig: $name already running"; return 0
    fi
    docker rm -f "$name" >/dev/null 2>&1 || true
    docker run -d --name "$name" \
        -p 127.0.0.1:${port}:5222 \
        -v "$HERE/prosody.cfg.lua:/etc/prosody/prosody.cfg.lua:ro" \
        "$IMAGE" >/dev/null
    for i in $(seq 1 30); do
        if (echo > /dev/tcp/127.0.0.1/${port}) 2>/dev/null; then break; fi
        sleep 0.5
    done
    for acct in "$@"; do
        case "$acct" in bot|bot2) pw="$BOT_PW" ;; admin) pw="$ADMIN_PW" ;; guest) pw="$GUEST_PW" ;; esac
        docker exec -u prosody "$name" prosodyctl register "$acct" localhost "$pw" >/dev/null
    done
    echo "rig: $name up on 127.0.0.1:${port} with accounts $* @localhost"
}
up() {
    start_one "$NAME" "$PORT" bot admin guest
    start_one "$NAME2" "$PORT2" bot2 admin guest
}
down() {
    docker rm -f "$NAME" "$NAME2" >/dev/null 2>&1 && echo "rig: stopped" || echo "rig: not running"
}
status() {
    docker ps -a --filter "name=xmproxy-test-prosody" --format '{{.Names}} {{.Status}}'
}
case "${1:-}" in
    up) up ;; down) down ;; status) status ;;
    stop-primary) docker stop "$NAME" >/dev/null && echo "rig: primary stopped" ;;
    start-primary) docker start "$NAME" >/dev/null && echo "rig: primary started" ;;
    stop-fallback) docker stop "$NAME2" >/dev/null && echo "rig: fallback stopped" ;;
    start-fallback) docker start "$NAME2" >/dev/null && echo "rig: fallback started" ;;
    *) echo "usage: $0 up|down|status|stop-primary|start-primary|stop-fallback|start-fallback"; exit 2 ;;
esac
