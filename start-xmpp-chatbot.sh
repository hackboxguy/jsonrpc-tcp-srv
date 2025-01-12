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

# Start the xmpp-chatbot-server binary
exec /usr/local/bin/xmproxysrv --loginfile=/tmp/xmpp-login.txt
