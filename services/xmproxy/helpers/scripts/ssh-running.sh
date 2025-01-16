#!/bin/sh
DEF_SRVPORT=22
if [ -z "$1" ]; then
        SRVPORT=$DEF_SRVPORT
else
        if [ -f "$1" ]; then #check if server-port is in a file
                SRVPORT=$(cat $1)
        else
                SRVPORT=$1
        fi
fi
RES=$(ps |grep "[s]sh -p $SRVPORT")
if [ $? = 0 ]; then
        exit 0
else
        exit 1
fi
