#!/bin/sh

# Set the library path to include /usr/local/lib
export LD_LIBRARY_PATH=/usr/local/lib

# Create necessary directories
mkdir -p /var/run

# Generate SSH keys if they don't exist
if [ ! -f /root/.ssh/id_rsa ]; then
    mkdir -p /root/.ssh
    ssh-keygen -t rsa -f /root/.ssh/id_rsa -N ""
    chmod 700 /root/.ssh
    chmod 600 /root/.ssh/id_rsa
    chmod 644 /root/.ssh/id_rsa.pub
fi

# Generate SSH authorized_keys file if it doesnt exist
if [ ! -f /root/.ssh/authorized_keys ]; then
    touch /root/.ssh/authorized_keys
    chmod 644 /root/.ssh/authorized_keys
fi

# start ssh server
dropbear -F -E -R -p 22 &

#start sysmgr service
exec /usr/local/bin/sysmgr --syscfg=docker &

# copy xmpp-login secrets from /run/secrets to temp location
# TODO: check if existing /run/secrets/xmpp-login can be directly passed to xmproxysrv
cp /run/secrets/xmpp-login /tmp/xmpp-login.txt

# copy over default xmpp-alias-list if it doesnt exist
if [ ! -f /xmpp-data/xmpp-alias-list.txt ]; then
	cp /usr/local/etc/xmproxy/xmpp-alias-list.txt /xmpp-data/
fi

# Check if XMPP_AI_URL(ollama) environment variable is set
if [ -z "$XMPP_AI_URL" ]; then
	AI_URL_ARG=""
else
	AI_URL_ARG="--aiagent=$XMPP_AI_URL"
fi

# Check if XMPP_AI_MODEL environment variable is set
if [ -z "$XMPP_AI_MODEL" ]; then
        AI_MODEL_ARG=""
else
        AI_MODEL_ARG="--aimodel=$XMPP_AI_MODEL"
fi
# Start the xmpp-chatbot-server binary
exec /usr/local/bin/xmproxysrv --loginfile=/tmp/xmpp-login.txt --aliaslist=/xmpp-data/xmpp-alias-list.txt "$AI_URL_ARG" "$AI_MODEL_ARG"
