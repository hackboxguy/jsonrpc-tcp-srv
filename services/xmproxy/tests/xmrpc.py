#!/usr/bin/env python
"""Shell client for the xmproxy JSON-RPC over XMPP protocol (docs/protocol.md).

    xmrpc.py --jid me@x --pw PW --bot bot@x [--host H] [--port 5222] [--plaintext] METHOD [PARAMS_JSON]

Prints the reply and any notification arriving within --wait seconds.
"""
import argparse
import asyncio
import json
import os
import sys
import time
import warnings

warnings.filterwarnings("ignore", category=DeprecationWarning)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xmppclient import BotChat  # noqa: E402


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jid", required=True)
    ap.add_argument("--pw", required=True)
    ap.add_argument("--bot", required=True)
    ap.add_argument("--host")
    ap.add_argument("--port", type=int, default=5222)
    ap.add_argument("--plaintext", action="store_true", help="no TLS (local rig)")
    ap.add_argument("--id", default=None, help="request id (default: unique, derived from the clock; the bot replays the reply for a repeated id within 60 s)")
    ap.add_argument("--wait", type=float, default=1.0, help="seconds to wait for notifications after the reply")
    ap.add_argument("method")
    ap.add_argument("params", nargs="?")
    a = ap.parse_args()
    host = a.host or a.jid.split("@", 1)[1]
    rid = a.id if a.id is not None else str(int(time.time() * 1000) % 1000000000)
    req = {"jsonrpc": "2.0", "id": int(rid) if rid.isdigit() else rid, "method": a.method}
    if a.params:
        req["params"] = json.loads(a.params)
    async with BotChat(a.jid, a.pw, a.bot, host, a.port, plaintext=a.plaintext) as chat:
        replies = await chat.ask(json.dumps(req), quiet=a.wait, timeout=30, min_replies=1)
    for r in replies:
        try:
            print(json.dumps(json.loads(r), indent=2))
        except ValueError:
            print(r)
    return 0 if replies else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
