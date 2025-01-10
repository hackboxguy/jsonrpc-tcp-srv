#!/bin/bash
export LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/lib/
echo "user: ${XMPUSER}" > ${CMAKE_INSTALL_PREFIX}/etc/xmproxy/xmpp-login.txt
echo "pw: ${XMPPW}" >> ${CMAKE_INSTALL_PREFIX}/etc/xmproxy/xmpp-login.txt
echo "adminbuddy: ${XMPBUDDY}" >> ${CMAKE_INSTALL_PREFIX}/etc/xmproxy/xmpp-login.txt
nohup ${CMAKE_INSTALL_PREFIX}/bin/xmproxysrv --loginfile=${CMAKE_INSTALL_PREFIX}/etc/xmproxy/xmpp-login.txt >/dev/null 2>&1 &

