#!/usr/bin/env python
"""Golden regression for xmproxysrv text commands (PRD criterion S4).

Sends every case in cases.json as the admin account and compares the replies
with golden/<name>.txt. Run with --record to (re)write the golden files.

Usage: test_chat_regression.py [--record] [--only NAME] [--verbose]
Environment: XMPP_HOST, XMPP_PORT, ADMIN_PW (see rig/accounts.env).
"""
import argparse
import asyncio
import json
import logging
import os
import re
import sys
import warnings

warnings.filterwarnings("ignore", category=DeprecationWarning)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xmppclient import BotChat  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
GOLDEN = os.path.join(HERE, "golden")
SEP = "\n----- reply -----\n"


def normalize(text, rules):
    for pattern, repl in rules or []:
        text = re.sub(pattern, repl, text)
    return text


async def run(args):
    host = os.environ.get("XMPP_HOST", "127.0.0.1")
    port = int(os.environ.get("XMPP_PORT", "5222"))
    domain = os.environ.get("XMPP_DOMAIN", "localhost")
    admin_pw = os.environ.get("ADMIN_PW", "adminpw-test")
    with open(os.path.join(HERE, "cases.json")) as f:
        cases = json.load(f)["cases"]
    if args.only:
        cases = [c for c in cases if c["name"] == args.only]
    os.makedirs(GOLDEN, exist_ok=True)
    failed = []
    async with BotChat("admin@%s" % domain, admin_pw, "bot@%s" % domain, host, port) as chat:
        for case in cases:
            replies = await chat.ask(case["cmd"],
                                     quiet=case.get("quiet", 1.5),
                                     timeout=case.get("timeout", 10.0),
                                     min_replies=case.get("min_replies", 1))
            actual = SEP.join(replies)
            path = os.path.join(GOLDEN, case["name"] + ".txt")
            if args.record:
                with open(path, "w") as f:
                    f.write(actual)
                print("recorded %-24s %d reply(ies)" % (case["name"], len(replies)))
                continue
            if not os.path.exists(path):
                print("MISSING  %-24s no golden file (run with --record)" % case["name"])
                failed.append(case["name"])
                continue
            with open(path) as f:
                golden = f.read()
            rules = case.get("normalize")
            if normalize(actual, rules) == normalize(golden, rules):
                print("ok       %-24s %d reply(ies)" % (case["name"], len(replies)))
            else:
                print("FAIL     %-24s" % case["name"])
                print("  expected: %r" % normalize(golden, rules))
                print("  actual:   %r" % normalize(actual, rules))
                failed.append(case["name"])
    return failed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--record", action="store_true")
    ap.add_argument("--only")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.WARNING)
    failed = asyncio.run(run(args))
    if failed:
        print("\n%d case(s) failed: %s" % (len(failed), ", ".join(failed)))
        sys.exit(1)
    if not args.record:
        print("\nall chat regression cases passed")


if __name__ == "__main__":
    main()
