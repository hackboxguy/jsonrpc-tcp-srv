#!/usr/bin/env python
"""Bucket 4: JSON-RPC 2.0 over XMPP (docs/protocol.md).

Runs as admin and as guest (viewer). Covers every method, batches,
notifications, malformed input, authorization, async completion, duplicate
suppression and interleaving with chat.
"""
import asyncio
import json
import os
import sys
import time
import warnings

warnings.filterwarnings("ignore", category=DeprecationWarning)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xmppclient import BotChat  # noqa: E402

HOST = os.environ.get("XMPP_HOST", "127.0.0.1")
PORT = int(os.environ.get("XMPP_PORT", "5222"))
DOMAIN = os.environ.get("XMPP_DOMAIN", "localhost")
ADMIN_PW = os.environ.get("ADMIN_PW", "adminpw-test")
GUEST_PW = os.environ.get("GUEST_PW", "guestpw-test")
BOT = "bot@%s" % DOMAIN
GUEST = "guest@%s" % DOMAIN
fails = []


def check(name, cond, detail=""):
    if cond:
        print("ok       %s" % name)
    else:
        print("FAIL     %s %s" % (name, detail))
        fails.append(name)


async def rpc(chat, body, quiet=0.8, timeout=10.0, min_replies=1):
    """send a raw body (str or object) and return parsed JSON replies."""
    if not isinstance(body, str):
        body = json.dumps(body)
    replies = await chat.ask(body, quiet=quiet, timeout=timeout, min_replies=min_replies)
    out = []
    for r in replies:
        try:
            out.append(json.loads(r))
        except ValueError:
            out.append(r)
    return out


def req(id_, method, params=None):
    d = {"jsonrpc": "2.0", "id": id_, "method": method}
    if params is not None:
        d["params"] = params
    return d


async def main():
    async with BotChat("admin@%s" % DOMAIN, ADMIN_PW, BOT, HOST, PORT) as admin, \
               BotChat(GUEST, GUEST_PW, BOT, HOST, PORT) as guest:
        await admin.ask("acceptbuddy %s" % GUEST)
        guest.send_presence(pto=BOT, ptype="subscribe")
        await asyncio.sleep(2.0)
        await admin.ask("acl %s remove" % GUEST)

        # ping
        r = await rpc(admin, req(1, "ping"))
        check("ping", len(r) == 1 and r[0].get("result", {}).get("pong") is True and r[0]["id"] == 1, repr(r))
        # describe
        r = await rpc(admin, req("d1", "describe"))
        d = r[0].get("result", {}) if r else {}
        check("describe_fields", d.get("protocol") == 1 and d.get("jid") == BOT and d.get("primary") == BOT and d.get("role") == "admin" and "exec" in d.get("methods", []) and "task.done" in d.get("notifications", []) and r[0]["id"] == "d1", repr(r))
        r = await rpc(guest, req(2, "describe"))
        check("describe_role_viewer", r and r[0].get("result", {}).get("role") == "viewer", repr(r))
        # list_commands
        r = await rpc(guest, req(3, "list_commands"))
        cmds = r[0].get("result", []) if r else []
        by = {c["name"]: c for c in cmds if isinstance(c, dict)}
        check("list_commands_shape", "account" in by and "shellcmd" in by and set(by["account"]) == {"name", "args", "role", "allowed"}, repr(cmds[:2]))
        check("list_commands_allowed_for_viewer", by.get("account", {}).get("allowed") is True and by.get("shellcmd", {}).get("allowed") is False and by.get("shellcmd", {}).get("role") == "admin", repr(by.get("shellcmd")))
        # exec
        r = await rpc(admin, req(4, "exec", {"cmd": "account"}))
        res = r[0].get("result", {}) if r else {}
        check("exec_account", res.get("return") == "Success" and res.get("result") == BOT and res.get("results", [{}])[0].get("cmd") == "account", repr(r))
        r = await rpc(admin, req(5, "exec", {"cmd": "nosuchcmd 1"}))
        check("exec_unknown_command_is_result", r and r[0].get("result", {}).get("return") == "UnknownCmd", repr(r))
        r = await rpc(admin, req(6, "exec", {"cmd": "account;botname"}))
        check("exec_rejects_semicolon", r and r[0].get("error", {}).get("code") == -32602, repr(r))
        r = await rpc(admin, req(7, "exec", {}))
        check("exec_missing_cmd", r and r[0].get("error", {}).get("code") == -32602, repr(r))
        r = await rpc(admin, req(8, "exec", {"cmd": "greeting"}))  # alias from fixture -> account
        check("exec_alias", r and r[0].get("result", {}).get("result") == BOT and r[0]["result"]["results"][0]["cmd"] == "account", repr(r))
        # alias expanding to a batch runs all steps and reports each
        await admin.ask("alias jb=account;botname")
        r = await rpc(admin, req(9, "exec", {"cmd": "jb"}))
        res = r[0].get("result", {}) if r else {}
        check("exec_alias_batch", len(res.get("results", [])) == 2 and res["results"][1]["cmd"] == "botname" and res.get("return") == "Success", repr(r))
        await admin.ask("alias jb=")
        # authorization
        r = await rpc(guest, req(10, "exec", {"cmd": "shellcmd ls"}))
        e = r[0].get("error", {}) if r else {}
        check("exec_denied_viewer", e.get("code") == -32001 and e.get("data", {}).get("requires") == "admin" and e["data"]["cmd"] == "shellcmd ls", repr(r))
        r = await rpc(guest, req(11, "exec", {"cmd": "account"}))
        check("exec_allowed_viewer", r and r[0].get("result", {}).get("return") == "Success", repr(r))
        # denied alias batch runs nothing: alias with allowed+denied steps
        await admin.ask("alias jd=account;sleep 1")
        t0 = time.monotonic()
        r = await rpc(guest, req(12, "exec", {"cmd": "jd"}))
        check("exec_denied_batch_runs_nothing", r and r[0].get("error", {}).get("code") == -32001 and time.monotonic() - t0 < 1.5, repr(r))
        await admin.ask("alias jd=")
        # async command -> InProgress + task.done notification
        r = await rpc(admin, req(13, "exec", {"cmd": "identify"}), quiet=2.0, timeout=15, min_replies=2)
        first = r[0] if r else {}
        res = first.get("result", {}) if isinstance(first, dict) else {}
        notes = [x for x in r[1:] if isinstance(x, dict) and x.get("method") == "task.done"]
        check("exec_async_inprogress", res.get("return") == "InProgress" and isinstance(res.get("task"), int), repr(first))
        check("exec_async_task_done", notes and notes[0]["params"].get("task") == res.get("task") and notes[0]["params"].get("id") == 13 and notes[0]["params"].get("return") == "Success", repr(r))
        # batch
        r = await rpc(admin, [req(20, "ping"), req(21, "exec", {"cmd": "account"}), {"jsonrpc": "2.0", "method": "ping"}])
        check("batch_replies_in_order", len(r) == 1 and isinstance(r[0], list) and [x["id"] for x in r[0]] == [20, 21] and r[0][1]["result"]["result"] == BOT, repr(r))
        # notification only -> no reply
        r = await rpc(admin, {"jsonrpc": "2.0", "method": "ping"}, timeout=3.0)
        check("notification_gets_no_reply", r == [], repr(r))
        # malformed
        r = await rpc(admin, '{"jsonrpc": "2.0", "id": 30, "method": ')
        check("parse_error", r and r[0].get("error", {}).get("code") == -32700 and r[0].get("id") is None, repr(r))
        r = await rpc(admin, '{"jsonrpc": "2.0", "id": 31}')
        check("invalid_request_no_method", r and r[0].get("error", {}).get("code") == -32600 and r[0].get("id") == 31, repr(r))
        r = await rpc(admin, '{"jsonrpc": "1.0", "id": 32, "method": "ping"}')
        check("invalid_request_bad_version", r and r[0].get("error", {}).get("code") == -32600, repr(r))
        r = await rpc(admin, req(33, "nosuch"))
        check("method_not_found", r and r[0].get("error", {}).get("code") == -32601, repr(r))
        r = await rpc(admin, "[]")
        check("empty_batch_invalid", r and r[0].get("error", {}).get("code") == -32600, repr(r))
        r = await rpc(admin, "[1, 2]")
        check("batch_of_non_objects", r and isinstance(r[0], list) and all(x.get("error", {}).get("code") == -32600 for x in r[0]), repr(r))
        r = await rpc(admin, '{"id": 34, "method": "ping"}')
        check("jsonrpc_field_optional", r and r[0].get("result", {}).get("pong") is True, repr(r))
        # duplicate suppression: same id twice -> cached reply, not re-run
        r1 = await rpc(admin, req(40, "exec", {"cmd": "sleep 1"}), timeout=10)
        t0 = time.monotonic()
        r2 = await rpc(admin, req(40, "exec", {"cmd": "sleep 1"}), timeout=10)
        dt = time.monotonic() - t0
        check("duplicate_id_replays_reply", r1 == r2 and r1 and r1[0].get("result", {}).get("return") == "Success" and dt < 1.0, "dt=%.2f %r %r" % (dt, r1, r2))
        r3 = await rpc(admin, req(41, "exec", {"cmd": "sleep 1"}), timeout=10)
        check("fresh_id_runs_again", r3 and r3[0].get("result", {}).get("return") == "Success", repr(r3))
        # chat still works, interleaved
        r = await admin.ask("account")
        check("chat_unchanged_after_json", r == ["return=Success : result=%s" % BOT], repr(r))
        r = await admin.ask("{not json")
        check("broken_brace_body_is_parse_error", r and '"code":-32700' in r[0], repr(r))
    print("jsonrpc: %s" % ("passed" if not fails else "FAILED"))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
