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

while true
do
        RES=$(ps | grep "[s]sh-start.sh")
        if [ $? = 0 ]; then
                MYPID=$(echo $RES | awk '{print $1}')
                kill $MYPID
        fi
        RES=$(ps | grep "[s]sh -p $SRVPORT")
        if [ $? = 0 ]; then
                MYPID=$(echo $RES | awk '{print $1}')
                kill $MYPID
        else
                break
        fi
done
