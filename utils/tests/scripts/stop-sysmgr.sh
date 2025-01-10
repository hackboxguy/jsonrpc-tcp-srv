#!/bin/bash
export LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/lib/
${CMAKE_INSTALL_PREFIX}/bin/sysmgrclt --shutdown > /dev/null
