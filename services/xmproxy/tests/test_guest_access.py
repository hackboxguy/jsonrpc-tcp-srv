#!/usr/bin/env python
"""Pins the current authorization behavior of xmproxysrv (baseline for bucket 3).

1. A guest that is not in the bot's roster gets no reply at all.
2. After the admin runs `acceptbuddy guest@...` and the guest sends a
   subscription request, the guest is in the roster and gets replies.
Environment: XMPP_HOST, XMPP_PORT, XMPP_DOMAIN, ADMIN_PW, GUEST_PW.
"""
import asyncio
import logging
import os
import sys
import warnings

warnings.filterwarnings("ignore", category=DeprecationWarning)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xmppclient import BotChat  # noqa: E402


async def run():
    host = os.environ.get("XMPP_HOST", "127.0.0.1")
    port = int(os.environ.get("XMPP_PORT", "5222"))
    domain = os.environ.get("XMPP_DOMAIN", "localhost")
    admin_pw = os.environ.get("ADMIN_PW", "adminpw-test")
    guest_pw = os.environ.get("GUEST_PW", "guestpw-test")
    bot = "bot@%s" % domain
    guest_jid = "guest@%s" % domain
    failures = []

    async with BotChat(guest_jid, guest_pw, bot, host, port) as guest:
        replies = await guest.ask("account", timeout=4.0, min_replies=1)
        if replies:
            failures.append("unknown guest got a reply: %r" % replies)
        else:
            print("ok       guest_ignored_when_not_in_roster")

        async with BotChat("admin@%s" % domain, admin_pw, bot, host, port) as admin:
            replies = await admin.ask("acceptbuddy %s" % guest_jid)
            if not replies or not replies[0].startswith("return=Success"):
                failures.append("acceptbuddy failed: %r" % replies)
            else:
                print("ok       admin_acceptbuddy")
            replies = await admin.ask("acceptbuddylist")
            if not replies or guest_jid not in replies[0]:
                failures.append("acceptbuddylist missing guest: %r" % replies)
            else:
                print("ok       admin_acceptbuddylist_contains_guest")

        # guest asks for subscription; the bot accepts pre-approved buddies
        guest.send_presence(pto=bot, ptype="subscribe")
        await asyncio.sleep(2.0)
        replies = await guest.ask("account", timeout=6.0, min_replies=1)
        if replies and replies[0].startswith("return=Success"):
            print("ok       guest_answered_after_acceptance")
        else:
            failures.append("accepted guest got no/unexpected reply: %r" % replies)

    for f in failures:
        print("FAIL     " + f)
    return failures


if __name__ == "__main__":
    logging.basicConfig(level=logging.WARNING)
    failed = asyncio.run(run())
    if failed:
        sys.exit(1)
    print("\nall guest access cases passed")
