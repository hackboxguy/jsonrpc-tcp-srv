#!/bin/bash
export LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/lib/
${CMAKE_INSTALL_PREFIX}/bin/tcp-json-rpc-client --servertcpport=40001 --requests=${CMAKE_INSTALL_PREFIX}/tests/sysmgr-test/requests.txt --responses=${CMAKE_INSTALL_PREFIX}/tests/sysmgr-test/responses.txt --repeat=20 > /dev/null
exit 0
#--logsocket=yes >/dev/null 2>&1 &
#${CMAKE_INSTALL_PREFIX}/bin/sysmgr --logsocket=yes &
