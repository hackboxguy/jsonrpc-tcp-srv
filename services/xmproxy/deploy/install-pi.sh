#!/bin/bash
# Install xmproxysrv + sysmgr on Raspberry Pi OS Lite (bookworm) or Debian.
#   sudo ./install-pi.sh [--prefix /opt/xmproxy] [--no-build]
# Builds from this checkout (gloox with OpenSSL included), creates the
# xmproxy user, installs systemd units, /etc/xmproxy defaults and
# /var/lib/xmproxy state. Existing config files are never overwritten.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PREFIX=/opt/xmproxy; BUILD=1
while [ $# -gt 0 ]; do case "$1" in
    --prefix) shift; PREFIX="$1" ;; --no-build) BUILD=0 ;;
    *) echo "unknown option $1"; exit 2 ;; esac; shift; done
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 1; }

if [ "$BUILD" = 1 ]; then
    if command -v apt-get >/dev/null; then
        apt-get install -y --no-install-recommends build-essential cmake git pkg-config \
            libjson-c-dev libssl-dev libi2c-dev wget bzip2
    fi
    "$HERE/build-from-source.sh" --prefix "$PREFIX"
fi

id -u xmproxy >/dev/null 2>&1 || useradd --system --home /var/lib/xmproxy --shell /usr/sbin/nologin xmproxy
install -d -m 750 -o xmproxy -g xmproxy /var/lib/xmproxy
install -d -m 755 /etc/xmproxy
for f in xmpp-alias-list.txt xmpp-botname.txt xmpp-acl.txt xmpp-subscriptions.txt xmpp-evntsubscr.txt; do
    [ -f /var/lib/xmproxy/$f ] || { : > /var/lib/xmproxy/$f; chown xmproxy:xmproxy /var/lib/xmproxy/$f; }
done
[ -f /etc/xmproxy/xmproxy.env ] || install -m 644 "$HERE/xmproxy.env" /etc/xmproxy/xmproxy.env
[ -f /etc/xmproxy/manifest.json ] || install -m 644 "$HERE/../helpers/configs/manifest-pi.json" /etc/xmproxy/manifest.json
if [ ! -f /etc/xmproxy/xmpp-login.txt ]; then
    install -m 640 -o root -g xmproxy "$HERE/../srv/xmpp-login.txt" /etc/xmproxy/xmpp-login.txt
    echo "!! edit /etc/xmproxy/xmpp-login.txt with the bot account and admin buddy"
fi
chgrp xmproxy /etc/xmproxy/xmpp-login.txt; chmod 640 /etc/xmproxy/xmpp-login.txt
sed "s|/opt/xmproxy|$PREFIX|g" "$HERE/sysmgr.service" > /etc/systemd/system/sysmgr.service
sed "s|/opt/xmproxy|$PREFIX|g" "$HERE/xmproxysrv.service" > /etc/systemd/system/xmproxysrv.service
systemctl daemon-reload
systemctl enable sysmgr.service xmproxysrv.service
echo "installed. start with: systemctl start sysmgr xmproxysrv ; logs: journalctl -u xmproxysrv -f"
