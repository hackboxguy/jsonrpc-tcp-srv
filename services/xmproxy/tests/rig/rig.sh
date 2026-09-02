#!/bin/bash
# Local XMPP test rig for xmproxy: Prosody in Docker on 127.0.0.1:5222.
# Usage: rig.sh up | down | status
# Accounts created on "up": bot, admin, guest (all @localhost), passwords in
# accounts.env. Development only; never used on a device.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
NAME=xmproxy-test-prosody
IMAGE=prosody/prosody:latest
PORT=5222
source "$HERE/accounts.env"

up() {
    if docker ps --format '{{.Names}}' | grep -qx "$NAME"; then
        echo "rig: $NAME already running"; return 0
    fi
    docker rm -f "$NAME" >/dev/null 2>&1 || true
    docker run -d --name "$NAME" \
        -p 127.0.0.1:${PORT}:5222 \
        -v "$HERE/prosody.cfg.lua:/etc/prosody/prosody.cfg.lua:ro" \
        "$IMAGE" >/dev/null
    # wait for c2s port
    for i in $(seq 1 30); do
        if (echo > /dev/tcp/127.0.0.1/${PORT}) 2>/dev/null; then break; fi
        sleep 0.5
    done
    for acct in bot admin guest; do
        pwvar="${acct^^}_PW"
        docker exec -u prosody "$NAME" prosodyctl register "$acct" localhost "${!pwvar}" >/dev/null
    done
    echo "rig: prosody up on 127.0.0.1:${PORT} with accounts bot, admin, guest @localhost"
}
down() {
    docker rm -f "$NAME" >/dev/null 2>&1 && echo "rig: stopped" || echo "rig: not running"
}
status() {
    docker ps --filter "name=$NAME" --format '{{.Names}} {{.Status}}'
}
case "${1:-}" in
    up) up ;; down) down ;; status) status ;;
    *) echo "usage: $0 up|down|status"; exit 2 ;;
esac
