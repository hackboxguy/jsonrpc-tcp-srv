#!/usr/bin/env python
"""Bucket 2 failover test (PRD S3). Requires the two-server rig and a bot
started with the fixture login file (primary bot@localhost on :5222, fallback
bot2@localhost on :5223, fallbackafter 5, primaryprobe 10).

 1. primary down  -> bot answers as bot2@localhost on the fallback server
 2. primary back  -> bot answers as bot@localhost on the primary again
 3. both down, then primary back -> bot returns to the primary
Environment: XMPP_HOST, XMPP_PORT, XMPP_PORT2, XMPP_DOMAIN, ADMIN_PW, RUN_DIR.
"""
import asyncio
import os
import subprocess
import sys
import time
import warnings

warnings.filterwarnings("ignore", category=DeprecationWarning)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xmppclient import BotChat  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.environ.get("XMPP_HOST", "127.0.0.1")
PORT = int(os.environ.get("XMPP_PORT", "5222"))
PORT2 = int(os.environ.get("XMPP_PORT2", "5223"))
DOMAIN = os.environ.get("XMPP_DOMAIN", "localhost")
ADMIN_PW = os.environ.get("ADMIN_PW", "adminpw-test")
RUN = os.environ.get("RUN_DIR", os.path.join(HERE, ".run"))
RIG = os.path.join(HERE, "rig", "rig.sh")
FAILOVER_LIMIT = 120.0  # PRD S3: on fallback within 2 min
FAILBACK_LIMIT = 90.0   # PRD S3: back on primary within 10 min; rig probes every 10 s


def rig(cmd):
    subprocess.run([RIG, cmd], check=True, capture_output=True)


async def account_via(port, bot, timeout=8.0):
    """Ask 'account' through the server on `port`; None if no answer."""
    try:
        async with BotChat("admin@%s" % DOMAIN, ADMIN_PW, bot, HOST, port) as chat:
            r = await chat.ask("account", timeout=timeout)
            return r[0] if r else None
    except Exception as e:  # server down, auth timeout
        return None


async def wait_for(port, bot, expected, limit):
    t0 = time.monotonic()
    last = None
    while time.monotonic() - t0 < limit:
        last = await account_via(port, bot)
        if last == "return=Success : result=%s" % expected:
            return time.monotonic() - t0, last
        await asyncio.sleep(3)
    return None, last


def log_has(text):
    try:
        with open(os.path.join(RUN, "xmproxysrv.log")) as f:
            return text in f.read()
    except OSError:
        return False


async def main():
    fails = []
    bot1, bot2 = "bot@%s" % DOMAIN, "bot2@%s" % DOMAIN
    took, last = await wait_for(PORT, bot1, bot1, 30)
    if took is None:
        print("FAIL     bot not answering on primary before the test: %r" % last)
        return 1
    print("ok       on_primary_initially")

    # 1. primary down -> fallback
    rig("stop-primary")
    t0 = time.monotonic()
    took, last = await wait_for(PORT2, bot2, bot2, FAILOVER_LIMIT)
    if took is None:
        fails.append("no answer as %s on fallback within %.0f s (last %r)" % (bot2, FAILOVER_LIMIT, last))
    else:
        print("ok       failover_to_fallback (%.1f s after primary stopped)" % (time.monotonic() - t0))
    if not log_has("failing over to %s" % bot2):
        fails.append("log has no failover line")

    # 2. primary back -> failback
    rig("start-primary")
    t0 = time.monotonic()
    took, last = await wait_for(PORT, bot1, bot1, FAILBACK_LIMIT)
    if took is None:
        fails.append("not back on %s within %.0f s (last %r)" % (bot1, FAILBACK_LIMIT, last))
    else:
        print("ok       failback_to_primary (%.1f s after primary started)" % (time.monotonic() - t0))
    if not log_has("switching back to %s" % bot1):
        fails.append("log has no failback line")

    # 3. both down, then primary back
    rig("stop-primary")
    rig("stop-fallback")
    await asyncio.sleep(25)  # let it fail on both sides a few times
    rig("start-primary")
    t0 = time.monotonic()
    took, last = await wait_for(PORT, bot1, bot1, FAILBACK_LIMIT)
    if took is None:
        fails.append("after both servers were down, not back on %s within %.0f s (last %r)" % (bot1, FAILBACK_LIMIT, last))
    else:
        print("ok       recover_after_both_down (%.1f s after primary started)" % (time.monotonic() - t0))
    rig("start-fallback")

    for f in fails:
        print("FAIL     " + f)
    print("failover: %s" % ("passed" if not fails else "FAILED"))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
