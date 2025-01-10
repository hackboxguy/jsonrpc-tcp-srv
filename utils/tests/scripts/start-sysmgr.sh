#!/bin/bash
export LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/lib/
nohup ${CMAKE_INSTALL_PREFIX}/bin/sysmgr --emulation=yes > /dev/null 2>&1 &
#${CMAKE_INSTALL_PREFIX}/bin/sysmgr --logsocket=yes &
