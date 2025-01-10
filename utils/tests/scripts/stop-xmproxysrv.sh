#!/bin/bash
export LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/lib/
${CMAKE_INSTALL_PREFIX}/bin/xmproxyclt --shutdown > /dev/null
#echo "stopping xmproxysrv" > ${CMAKE_INSTALL_PREFIX}/tests/xmproxy-test/test-report.txt
