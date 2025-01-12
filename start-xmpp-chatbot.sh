#!/bin/sh

# Set the library path to include /usr/local/lib
export LD_LIBRARY_PATH=/usr/local/lib

#start sysmgr service
exec /usr/local/bin/sysmgr --syscfg=docker &

# copy xmpp-login secrets from /run/secrets to temp location
# TODO: check if existing /run/secrets/xmpp-login can be directly passed to xmproxysrv
cp /run/secrets/xmpp-login /tmp/xmpp-login.txt

# Start the xmpp-chatbot-server binary
exec /usr/local/bin/xmproxysrv --loginfile=/tmp/xmpp-login.txt
