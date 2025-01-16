#!/bin/sh
/usr/local/sbin/ssh-tunnel-start.sh --remoteuser=$1 --remoteserver=$2 --remoteport=$3 --remotetunnelport=$4 --localserver=$5 --localport=$6
