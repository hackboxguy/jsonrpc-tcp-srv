#!/bin/sh
export LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/lib/
LOOPCOUNT=5000
STATS=$(${CMAKE_INSTALL_PREFIX}/bin/tcp-json-rpc-client --servertcpport=40005 --requests=${CMAKE_INSTALL_PREFIX}/tests/xmproxy-test/requests.txt --responses=${CMAKE_INSTALL_PREFIX}/tests/xmproxy-test/responses.txt --repeat=$LOOPCOUNT)
if [ $? != "0" ]; then
	echo "$STATS" | grep mismatch
	echo "$STATS" > ${CMAKE_INSTALL_PREFIX}/tests/xmproxy-test/test-report.txt
	exit 1
else
	echo "$STATS" > ${CMAKE_INSTALL_PREFIX}/tests/xmproxy-test/test-report.txt
	exit 0
fi
