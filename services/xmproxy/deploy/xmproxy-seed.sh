#!/bin/sh
# Idempotent first-boot / every-boot seeding of the durable xmproxy state.
# Creates the etc and state directories under DATA_ROOT with the right
# ownership and copies any default file that is missing from DEFAULTS.
# Existing files are never touched, so user settings survive updates and the
# daemon always finds a complete set of files after a factory reset.
#
#   xmproxy-seed.sh [DATA_ROOT=/data/xmproxy] [DEFAULTS=/opt/xmproxy/share/xmproxy/etc]
set -eu
DATA_ROOT=${1:-${DATA_ROOT:-/data/xmproxy}}
DEFAULTS=${2:-${DEFAULTS:-/opt/xmproxy/share/xmproxy/etc}}
GID=$(getent group xmproxy | cut -d: -f3) || GID=0
install -d -m 0755 -o 0 -g 0 "$DATA_ROOT"
install -d -m 0750 -o 0 -g "$GID" "$DATA_ROOT/etc"
install -d -m 0750 -o xmproxy -g xmproxy "$DATA_ROOT/state"
for f in xmproxy.env manifest.json xmpp-login.txt; do
    [ -f "$DATA_ROOT/etc/$f" ] || [ ! -f "$DEFAULTS/$f" ] || install -m 0640 -o 0 -g "$GID" "$DEFAULTS/$f" "$DATA_ROOT/etc/$f"
done
chmod 0640 "$DATA_ROOT/etc/xmpp-login.txt" 2>/dev/null || true
for f in xmpp-alias-list.txt xmpp-botname.txt xmpp-acl.txt xmpp-subscriptions.txt xmpp-evntsubscr.txt; do
    [ -f "$DATA_ROOT/state/$f" ] || { : > "$DATA_ROOT/state/$f"; chown xmproxy:xmproxy "$DATA_ROOT/state/$f"; chmod 0640 "$DATA_ROOT/state/$f"; }
done
