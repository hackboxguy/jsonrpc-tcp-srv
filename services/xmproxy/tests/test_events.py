#!/usr/bin/env python
"""Bucket 6: subscriptions, indicator polling and notifications.

Fixture manifest controls used: name (text, botname, 5 s), sw (toggle on
botname, match ON), sh (indicator through an asynchronous shellcmd), who.
Login fixture sets heartbeat: 5.
"""
import asyncio
import json
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
DOMAIN = os.environ.get("XMPP_DOMAIN", "localhost")
ADMIN_PW = os.environ.get("ADMIN_PW", "adminpw-test")
GUEST_PW = os.environ.get("GUEST_PW", "guestpw-test")
RUN = os.environ.get("RUN_DIR", os.path.join(HERE, ".run"))
CONTAINER = os.environ.get("RIG_CONTAINER", "xmproxy-test-prosody")
BOT = "bot@%s" % DOMAIN
GUEST = "guest@%s" % DOMAIN
fails = []


def check(name, cond, detail=""):
    if cond:
        print("ok       %s" % name)
    else:
        print("FAIL     %s %s" % (name, detail))
        fails.append(name)


def parse(replies):
    out = []
    for r in replies:
        try:
            out.append(json.loads(r))
        except ValueError:
            out.append(r)
    return out


async def rpc(chat, method, params=None, timeout=10.0):
    req = {"jsonrpc": "2.0", "id": int(time.time() * 1000) % 1000000000, "method": method}
    if params is not None:
        req["params"] = params
    chat.send_to_bot(json.dumps(req))
    # wait for the reply carrying our id; keep events aside
    deadline = time.monotonic() + timeout
    events = []
    while time.monotonic() < deadline:
        try:
            sender, body = await asyncio.wait_for(chat._inbox.get(), timeout=deadline - time.monotonic())
        except asyncio.TimeoutError:
            break
        try:
            obj = json.loads(body)
        except ValueError:
            events.append(body)
            continue
        if isinstance(obj, dict) and obj.get("id") == req["id"]:
            for e in events:
                chat._inbox.put_nowait((sender, e if isinstance(e, str) else json.dumps(e)))
            return obj
        events.append(obj)
    return {}


async def events(chat, seconds, topic=None):
    """collect event notifications for `seconds`."""
    out = []
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            sender, body = await asyncio.wait_for(chat._inbox.get(), timeout=max(0.05, deadline - time.monotonic()))
        except asyncio.TimeoutError:
            break
        try:
            obj = json.loads(body)
        except ValueError:
            continue
        if isinstance(obj, dict) and obj.get("method") == "event":
            p = obj.get("params", {})
            if topic is None or p.get("topic") == topic:
                out.append(p)
    return out


def drain(chat):
    while not chat._inbox.empty():
        chat._inbox.get_nowait()


async def main():
    async with BotChat("admin@%s" % DOMAIN, ADMIN_PW, BOT, HOST, PORT) as admin, \
               BotChat(GUEST, GUEST_PW, BOT, HOST, PORT) as guest:
        await admin.ask("acceptbuddy %s" % GUEST)
        guest.send_presence(pto=BOT, ptype="subscribe")
        await asyncio.sleep(2.0)
        await admin.ask("acl %s remove" % GUEST)
        await admin.ask("botname xmproxy-test-bot")
        for c in (admin, guest):
            await rpc(c, "unsubscribe")
            drain(c)

        d = (await rpc(admin, "describe")).get("result", {})
        check("describe_lists_event_methods", "subscribe" in d.get("methods", []) and "event" in d.get("notifications", []) and d.get("heartbeat") == 5 and "system" in d.get("topics", []), repr(d))

        # subscribe -> initial value
        r = await rpc(admin, "subscribe", {"topics": ["name"]})
        check("subscribe_result", r.get("result", {}).get("topics") == ["name"], repr(r))
        ev = await events(admin, 6, "name")
        check("initial_value_on_subscribe", len(ev) >= 1 and ev[0].get("value") == "xmproxy-test-bot" and ev[0].get("control") == "name" and ev[0].get("type") == "text" and isinstance(ev[0].get("time"), int), repr(ev))
        # no change -> no event
        ev = await events(admin, 12, "name")
        check("no_event_without_change", ev == [], repr(ev))
        # change -> event within the interval
        t0 = time.monotonic()
        await admin.ask("botname renamed-bot")
        drain(admin)
        ev = await events(admin, 20, "name")  # interval 5 s, generous under sanitizers
        check("event_on_change", len(ev) == 1 and ev[0].get("value") == "renamed-bot", "%.1fs %r" % (time.monotonic() - t0, ev))
        # get_subscriptions / persistence
        r = await rpc(admin, "get_subscriptions")
        check("get_subscriptions", r.get("result", {}).get("topics") == ["name"], repr(r))
        with open(os.path.join(RUN, "xmpp-subscriptions.txt")) as f:
            content = f.read()
        check("subscriptions_persisted", "admin@%s name" % DOMAIN in content, content)
        # unsubscribe -> silence
        r = await rpc(admin, "unsubscribe", {"topics": ["name"]})
        check("unsubscribe_result", r.get("result", {}).get("topics") == [], repr(r))
        await admin.ask("botname xmproxy-test-bot")
        drain(admin)
        ev = await events(admin, 8, "name")
        check("no_event_after_unsubscribe", ev == [], repr(ev))

        # toggle state and async shell indicator, as guest (viewer)
        r = await rpc(guest, "subscribe", {"topics": ["sw", "sh"]})
        check("viewer_may_subscribe", r.get("result", {}).get("topics") == ["sh", "sw"], repr(r))
        ev = await events(guest, 8)
        by = {e["control"]: e for e in ev if "control" in e}
        check("toggle_initial_off", by.get("sw", {}).get("value") == "off", repr(ev))
        check("async_shell_indicator", by.get("sh", {}).get("value") == "hi-there", repr(ev))
        await admin.ask("run sw on")  # botname sw-ON
        drain(guest)
        ev = await events(guest, 12, "sw")
        check("toggle_event_on", len(ev) == 1 and ev[0].get("value") == "on", repr(ev))
        await admin.ask("botname xmproxy-test-bot")
        # invalid topics and role
        r = await rpc(guest, "subscribe", {"topics": ["nosuch"]})
        check("unknown_topic", r.get("error", {}).get("code") == -32602 and r["error"]["data"]["topic"] == "nosuch", repr(r))
        r = await rpc(guest, "subscribe", {"topics": ["blink"]})
        check("non_pollable_topic", r.get("error", {}).get("code") == -32602, repr(r))
        r = await rpc(guest, "subscribe", {})
        check("subscribe_bad_params", r.get("error", {}).get("code") == -32602, repr(r))
        await rpc(guest, "unsubscribe")
        drain(guest)

        # chat watch / unwatch
        r = (await guest.ask("watch name", quiet=0.5))[0]
        check("chat_watch", r == "return=Success : result=watching name", r)
        await asyncio.sleep(3)
        r = (await guest.ask("watch", quiet=0.5))[0]
        check("chat_watch_list", "name" in r, r)
        r = (await guest.ask("watch blink", quiet=0.5))[0]
        check("chat_watch_invalid", r.startswith("return=ArgError"), r)
        r = (await guest.ask("unwatch", quiet=0.5))[0]
        check("chat_unwatch_all", r == "return=Success : result=no subscriptions", r)
        drain(guest)

        # star subscription, task topic and heartbeat
        r = await rpc(guest, "subscribe", {"topics": ["*"]})
        check("star_subscribe", r.get("result", {}).get("topics") == ["*"], repr(r))
        ev = await events(guest, 8)
        topics = set(e.get("topic") for e in ev)
        check("star_gets_all_indicators", {"name", "sh", "sw", "who", "up"} <= topics, repr(topics))
        drain(guest)
        await admin.ask("identify")  # async task started by the admin
        ev = await events(guest, 10, "task")
        check("task_topic_for_others", len(ev) >= 1 and ev[0].get("requester") == "admin@%s" % DOMAIN and ev[0].get("return") == "Success", repr(ev))
        drain(guest)
        ev = await events(guest, 8, "heartbeat")
        check("heartbeat_topic", len(ev) >= 1 and isinstance(ev[0].get("uptime"), int) and ev[0].get("jid") == BOT and ev[0].get("on_fallback") is False, repr(ev))
        await rpc(guest, "unsubscribe")
        drain(guest)

        # system topic: online after a server restart
        r = await rpc(admin, "subscribe", {"topics": ["system"]})
        check("system_subscribe", r.get("result", {}).get("topics") == ["system"], repr(r))
        drain(admin)
        subprocess.run(["docker", "restart", CONTAINER], check=True, capture_output=True)
        try:
            await admin.wait_ready(60)
        except asyncio.TimeoutError:
            pass
        ev = await events(admin, 30, "system")
        check("system_online_event", any(e.get("event") == "online" and e.get("jid") == BOT for e in ev), repr(ev))
        await rpc(admin, "unsubscribe")

    try:
        with open(os.path.join(RUN, "xmproxysrv.log")) as f:
            log = f.read()
        check("poll_logged", "events: 'name' = 'renamed-bot'" in log)
    except OSError:
        check("poll_logged", False)
    print("events: %s" % ("passed" if not fails else "FAILED"))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
