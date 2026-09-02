#!/usr/bin/env python
"""Bucket 1 resilience checks against the running rig:
 1. XMPP server restart: bot is back online (TCP RPC) within 30 s and answers.
 2. Peer restart (F2): sysmgr is killed and restarted; within the
    re-subscribe period an async command (identify) completes again.
 3. Bounded shutdown: SIGTERM to xmproxysrv exits within 5 s.
Environment: RUN_DIR (pid files, logs), SYSMGR_BIN, XMPP_* and ADMIN_PW.
Must run last: it stops xmproxysrv.
"""
import asyncio
import json
import os
import signal
import socket
import subprocess
import sys
import time
import warnings

warnings.filterwarnings("ignore", category=DeprecationWarning)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xmppclient import BotChat  # noqa: E402

HOST = os.environ.get("XMPP_HOST", "127.0.0.1")
PORT = int(os.environ.get("XMPP_PORT", "5222"))
DOMAIN = os.environ.get("XMPP_DOMAIN", "localhost")
BOT = "bot@%s" % DOMAIN
RUN = os.environ.get("RUN_DIR", os.path.join(os.path.dirname(os.path.abspath(__file__)), ".run"))
CONTAINER = os.environ.get("RIG_CONTAINER", "xmproxy-test-prosody")
RPC_PORT = int(os.environ.get("XMPROXY_PORT", "40005"))


def rpc_online():
    try:
        s = socket.create_connection(("127.0.0.1", RPC_PORT), 2)
        s.sendall(b'{ "jsonrpc": "2.0", "method": "get_online_status", "id": 0 }\n')
        data = s.recv(4096)
        s.close()
        return b'"online"' in data
    except OSError:
        return False


def wait_online(timeout):
    t0 = time.monotonic()
    while time.monotonic() - t0 < timeout:
        if rpc_online():
            return time.monotonic() - t0
        time.sleep(0.5)
    return None


def pid(name):
    with open(os.path.join(RUN, name + ".pid")) as f:
        return int(f.read().strip())


def alive(p):
    try:
        os.kill(p, 0)
        return True
    except OSError:
        return False


async def main():
    admin_pw = os.environ.get("ADMIN_PW", "adminpw-test")
    fails = []

    # 1. server restart
    subprocess.run(["docker", "restart", CONTAINER], check=True, capture_output=True)
    time.sleep(1.0)
    took = wait_online(30)
    if took is None:
        fails.append("bot not back online 30 s after server restart")
    else:
        print("ok       reconnect_after_server_restart (%.1f s)" % took)
    async with BotChat("admin@%s" % DOMAIN, admin_pw, BOT, HOST, PORT) as admin:
        r = await admin.ask("account")
        if r and r[0].startswith("return=Success"):
            print("ok       answers_after_server_restart")
        else:
            fails.append("no answer after server restart: %r" % r)

        # 2. peer restart
        sysmgr_bin = os.environ.get("SYSMGR_BIN")
        if sysmgr_bin:
            old = pid("sysmgr")
            os.kill(old, signal.SIGKILL)
            time.sleep(1.0)
            with open(os.path.join(RUN, "sysmgr.log"), "a") as log:
                proc = subprocess.Popen([sysmgr_bin, "--syscfg=docker", "--emulation"], stdout=log, stderr=subprocess.STDOUT)
            with open(os.path.join(RUN, "sysmgr.pid"), "w") as f:
                f.write(str(proc.pid))
            print("peer:    sysmgr killed and restarted, waiting for re-subscribe")
            deadline = time.monotonic() + 45
            done = False
            while time.monotonic() < deadline:
                r = await admin.ask("identify", timeout=12, min_replies=2)
                if len(r) >= 2 and r[1].startswith("return=Success"):
                    done = True
                    break
                await asyncio.sleep(5)
            if done:
                print("ok       async_completion_after_peer_restart")
            else:
                fails.append("identify never completed after sysmgr restart: %r" % r)
        else:
            print("skip     peer restart (SYSMGR_BIN not set)")

    # 3. bounded shutdown
    xp = pid("xmproxysrv")
    t0 = time.monotonic()
    os.kill(xp, signal.SIGTERM)
    while alive(xp) and time.monotonic() - t0 < 10:
        time.sleep(0.1)
    took = time.monotonic() - t0
    if alive(xp):
        fails.append("xmproxysrv still alive 10 s after SIGTERM")
        os.kill(xp, signal.SIGKILL)
    elif took > 5.0:
        fails.append("xmproxysrv took %.1f s to exit (limit 5 s)" % took)
    else:
        print("ok       sigterm_exit (%.1f s)" % took)

    for f in fails:
        print("FAIL     " + f)
    print("resilience: %s" % ("passed" if not fails else "FAILED"))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
