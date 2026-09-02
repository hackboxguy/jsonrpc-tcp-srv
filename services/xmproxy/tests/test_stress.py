#!/usr/bin/env python
"""Bucket 1 stress test: two senders hammer the bot while the XMPP server is
restarted twice. Passes when no reply is lost, no extra reply appears and the
bot is back online at the end.

Environment: XMPP_HOST, XMPP_PORT, XMPP_DOMAIN, ADMIN_PW, GUEST_PW,
STRESS_COUNT (commands per sender, default 250), STRESS_RESTARTS (default 2),
RIG_CONTAINER (default xmproxy-test-prosody).
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

HOST = os.environ.get("XMPP_HOST", "127.0.0.1")
PORT = int(os.environ.get("XMPP_PORT", "5222"))
DOMAIN = os.environ.get("XMPP_DOMAIN", "localhost")
BOT = "bot@%s" % DOMAIN
COUNT = int(os.environ.get("STRESS_COUNT", "250"))
RESTARTS = int(os.environ.get("STRESS_RESTARTS", "2"))
CONTAINER = os.environ.get("RIG_CONTAINER", "xmproxy-test-prosody")
REPLY_TIMEOUT = 40.0


async def sender(name, chat, stats):
    for i in range(COUNT):
        if not chat._ready.is_set():
            try:
                await chat.wait_ready(60)
            except asyncio.TimeoutError:
                stats["lost"] += COUNT - i
                stats["note"] = "client could not reconnect"
                return
        t0 = time.monotonic()
        chat.send_to_bot("account")
        replies = await chat.collect(quiet=0.0, timeout=REPLY_TIMEOUT, min_replies=1)
        dt = time.monotonic() - t0
        if not replies:
            stats["lost"] += 1
            continue
        stats["latency"].append(dt)
        if len(replies) > 1:
            stats["extra"] += len(replies) - 1
        if not replies[0].startswith("return=Success"):
            stats["bad"] += 1
    stats["done"] = True


async def chaos(chats):
    delay = max(5.0, COUNT * 0.04)
    for n in range(RESTARTS):
        await asyncio.sleep(delay)
        print("chaos: restarting %s (%d/%d)" % (CONTAINER, n + 1, RESTARTS))
        subprocess.run(["docker", "restart", CONTAINER], check=False, capture_output=True)


def pct(v, p):
    if not v:
        return 0.0
    v = sorted(v)
    return v[min(len(v) - 1, int(len(v) * p))]


async def main():
    admin_pw = os.environ.get("ADMIN_PW", "adminpw-test")
    guest_pw = os.environ.get("GUEST_PW", "guestpw-test")
    ok = True
    async with BotChat("admin@%s" % DOMAIN, admin_pw, BOT, HOST, PORT) as admin:
        # make the guest a buddy so it is answered too
        await admin.ask("acceptbuddy guest@%s" % DOMAIN)
        async with BotChat("guest@%s" % DOMAIN, guest_pw, BOT, HOST, PORT) as guest:
            guest.send_presence(pto=BOT, ptype="subscribe")
            await asyncio.sleep(2.0)
            if not await guest.ask("account"):
                print("FAIL     guest not answered before stress")
                return 1
            stats = {n: {"lost": 0, "extra": 0, "bad": 0, "latency": [], "done": False} for n in ("admin", "guest")}
            t0 = time.monotonic()
            await asyncio.gather(sender("admin", admin, stats["admin"]),
                                 sender("guest", guest, stats["guest"]),
                                 chaos([admin, guest]))
            total = time.monotonic() - t0
            for n, st in stats.items():
                lat = st["latency"]
                print("%-6s sent=%d answered=%d lost=%d extra=%d bad=%d  latency median=%.3fs p95=%.3fs max=%.3fs  client reconnects=%d %s" % (
                    n, COUNT, len(lat), st["lost"], st["extra"], st["bad"], pct(lat, 0.5), pct(lat, 0.95), max(lat) if lat else 0,
                    (admin if n == "admin" else guest).disconnects, st.get("note", "")))
                if st["lost"] or st["extra"] or st["bad"] or not st["done"]:
                    ok = False
            print("total %.1fs for %d commands with %d server restart(s)" % (total, 2 * COUNT, RESTARTS))
            # bot must be back and responsive
            await asyncio.sleep(1.0)
            if not await admin.ask("echo", timeout=20):
                print("FAIL     bot not responsive after stress")
                ok = False
    print("stress: %s" % ("passed" if ok else "FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
