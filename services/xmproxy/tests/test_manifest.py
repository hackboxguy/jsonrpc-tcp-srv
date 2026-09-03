#!/usr/bin/env python
"""Bucket 5: device-served manifest (docs/manifest.md, PRD S6).

Requires the daemon started with --manifest RUN_DIR/manifest.json (copied
from fixtures/manifest.json by run-tests.sh).
"""
import asyncio
import json
import os
import shutil
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
MANIFEST = os.path.join(RUN, "manifest.json")
FIXTURE = os.path.join(HERE, "fixtures", "manifest.json")
BOT = "bot@%s" % DOMAIN
GUEST = "guest@%s" % DOMAIN
fails = []


def check(name, cond, detail=""):
    if cond:
        print("ok       %s" % name)
    else:
        print("FAIL     %s %s" % (name, detail))
        fails.append(name)


async def rpc(chat, method, params=None, quiet=0.8, timeout=10.0, min_replies=1):
    req = {"jsonrpc": "2.0", "id": int(time.time() * 1000) % 1000000000, "method": method}
    if params is not None:
        req["params"] = params
    out = []
    for r in await chat.ask(json.dumps(req), quiet=quiet, timeout=timeout, min_replies=min_replies):
        try:
            out.append(json.loads(r))
        except ValueError:
            out.append(r)
    return out


def first(x):
    return x[0] if x else {}


def write_manifest(text):
    with open(MANIFEST, "w") as f:
        f.write(text)


async def main():
    async with BotChat("admin@%s" % DOMAIN, ADMIN_PW, BOT, HOST, PORT) as admin, \
               BotChat(GUEST, GUEST_PW, BOT, HOST, PORT) as guest:
        await admin.ask("acceptbuddy %s" % GUEST)
        guest.send_presence(pto=BOT, ptype="subscribe")
        await asyncio.sleep(2.0)
        await admin.ask("acl %s remove" % GUEST)
        shutil.copy(FIXTURE, MANIFEST)
        r = (await admin.ask("manifest reload"))[0]
        check("reload_fixture", r.startswith("return=Success"), r)

        # describe advertises the manifest
        d = first(await rpc(admin, "describe")).get("result", {})
        check("describe_advertises_manifest", d.get("manifest") is True and "get_manifest" in d.get("methods", []), repr(d))

        # get_manifest as admin
        m = first(await rpc(admin, "get_manifest")).get("result", {})
        ctl = {c["id"]: c for g in m.get("groups", []) for c in g.get("controls", [])}
        check("manifest_structure", m.get("manifest") == 1 and m.get("device", {}).get("name") == "rig-device" and [g["id"] for g in m["groups"]] == ["status", "actions"] and len(ctl) == 7, repr(m)[:300])
        check("manifest_defaults", ctl["blink"]["role"] == "operator" and ctl["who"]["role"] == "viewer" and ctl["restart"]["confirm"] is True and ctl["blink"]["confirm"] is False and ctl["up"]["unit"] == "h" and ctl["led"]["match"] == "1", repr(ctl.get("blink")))
        check("manifest_allowed_admin", all(c["allowed"] for c in ctl.values()))
        # as guest (viewer)
        m = first(await rpc(guest, "get_manifest")).get("result", {})
        ctl = {c["id"]: c for g in m.get("groups", []) for c in g.get("controls", [])}
        check("manifest_allowed_viewer", ctl["who"]["allowed"] and ctl["nap"]["allowed"] and not ctl["blink"]["allowed"] and not ctl["restart"]["allowed"] and not ctl["led"]["allowed"], repr({k: v["allowed"] for k, v in ctl.items()}))

        # the grant: sleep is operator when typed, but control 'nap' is granted to viewers
        r = first(await rpc(guest, "exec", {"cmd": "sleep 1"}))
        check("typed_sleep_denied_for_viewer", r.get("error", {}).get("code") == -32001, repr(r))
        r = first(await rpc(guest, "exec", {"control": "nap"}, timeout=10)).get("result", {})
        check("control_grant_lets_viewer_run", r.get("return") == "Success" and r.get("control") == "nap" and r["results"][0]["cmd"] == "sleep 1", repr(r))
        r = first(await rpc(guest, "exec", {"control": "blink"}))
        check("control_role_enforced", r.get("error", {}).get("code") == -32001 and r["error"]["data"]["requires"] == "operator" and r["error"]["data"]["control"] == "blink", repr(r))
        r = first(await rpc(guest, "exec", {"control": "nope"}))
        check("unknown_control", r.get("error", {}).get("code") == -32602 and r["error"]["data"]["control"] == "nope", repr(r))
        r = first(await rpc(guest, "exec", {"control": "greet"})).get("result", {})
        check("control_alias_expands", r.get("results", [{}])[0].get("cmd") == "account" and r.get("result") == BOT, repr(r))
        # toggle
        r = first(await rpc(admin, "exec", {"control": "led"}))
        check("toggle_without_arg_reads_state", r.get("result", {}).get("results", [{}])[0].get("cmd") == "gpio 4", repr(r))
        r = first(await rpc(admin, "exec", {"control": "led", "arg": "on"}))
        check("toggle_on_runs_on_command", r.get("result", {}).get("results", [{}])[0].get("cmd") == "gpio 4 1", repr(r))
        r = first(await rpc(admin, "exec", {"control": "led", "arg": "sideways"}))
        check("toggle_bad_arg", r.get("error", {}).get("code") == -32602, repr(r))
        # async control -> task.done
        rs = await rpc(admin, "exec", {"control": "blink"}, quiet=2.0, timeout=15, min_replies=2)
        res = first(rs).get("result", {})
        notes = [x for x in rs[1:] if isinstance(x, dict) and x.get("method") == "task.done"]
        check("async_control_task_done", res.get("return") == "InProgress" and notes and notes[0]["params"]["task"] == res.get("task"), repr(rs))
        # chat: run
        r = (await guest.ask("run nap", timeout=10))[0]
        check("chat_run_granted", r == "return=Success : result=", r)
        r = (await guest.ask("run blink"))[0]
        check("chat_run_denied", r == "return=ActionBlocked : result=requires operator", r)
        r = (await guest.ask("run nope"))[0]
        check("chat_run_unknown", r.startswith("return=ItemNotFound"), r)
        r = (await admin.ask("run led on"))[0]
        check("chat_run_toggle", r.startswith("return=HostUnreachable"), r)  # no gpio service in the rig
        r = (await admin.ask("run greet"))[0]
        check("chat_run_alias", r == "return=Success : result=%s" % BOT, r)
        # chat: manifest summary and admin-only subcommands
        r = (await guest.ask("manifest"))[0]
        check("manifest_summary_viewer", "device rig-device" in r and "controls 7" in r, r)
        r = (await guest.ask("manifest reload"))[0]
        check("manifest_reload_admin_only", r == "return=ActionBlocked : result=requires admin", r)
        # invalid file: reload fails, previous manifest kept (S6)
        write_manifest('{"manifest": 1, "device": {"name": "x"}, "groups": [{"id": "g", "controls": [{"id": "a", "type": "button"}]}]}')
        r = (await admin.ask("manifest check"))[0]
        check("check_reports_error", r.startswith("return=Fail") and "button needs 'action'" in r, r)
        r = (await admin.ask("manifest reload"))[0]
        check("reload_fails_keeps_previous", r.startswith("return=Fail") and "previous manifest kept" in r, r)
        m = first(await rpc(admin, "get_manifest")).get("result", {})
        check("previous_manifest_still_served", m.get("device", {}).get("name") == "rig-device", repr(m)[:100])
        cases = [
            ('{"manifest": 2, "device": {"name": "x"}, "groups": []}', "'manifest' must be 1"),
            ('{"manifest": 1, "groups": []}', "'device.name' is required"),
            ('{"manifest": 1, "device": {"name": "x"}, "groups": [{"id": "g", "controls": [{"id": "a", "type": "dial", "action": "x"}]}]}', "'type' must be"),
            ('{"manifest": 1, "device": {"name": "x"}, "groups": [{"id": "g", "controls": [{"id": "a", "type": "button", "action": "x"}, {"id": "a", "type": "button", "action": "y"}]}]}', "duplicate control id"),
            ('{"manifest": 1, "device": {"name": "x"}, "groups": [{"id": "g", "controls": [{"id": "a", "type": "indicator", "command": "uptime", "interval": 2}]}]}', "'interval' must be"),
            ('{"manifest": 1, "device": {"name": "x"}, "groups": [{"id": "g", "controls": [{"id": "a", "type": "button", "action": "account;botname"}]}]}', "must be one command"),
            ('{"manifest": 1, "device": {"name": "x"}, "groups": [{"id": "g", "controls": [{"id": "a", "type": "toggle", "on": "x"}]}]}', "toggle needs 'on' and 'off'"),
            ('{"manifest": 1, "device": {"name": "x"}, "groups": [{"id": "g", "controls": [{"id": "a b", "type": "button", "action": "x"}]}]}', "'id' missing or invalid"),
            ('{"manifest": 1, "device": {"name": "x"}, "groups": [{"id": "g", "controls": [{"id": "a", "type": "button", "action": "x", "role": "boss"}]}]}', "'role' must be"),
            ('{not json', "invalid JSON"),
        ]
        for text, expect in cases:
            write_manifest(text)
            r = (await admin.ask("manifest check"))[0]
            check("validation: %s" % expect, r.startswith("return=Fail") and expect in r, r)
        # unresolved command is a warning, not an error
        write_manifest('{"manifest": 1, "device": {"name": "y"}, "groups": [{"id": "g", "controls": [{"id": "a", "type": "button", "action": "nosuchalias"}]}]}')
        r = (await admin.ask("manifest reload"))[0]
        check("unresolved_command_is_warning", r.startswith("return=Success") and "not a known command or alias" in r, r)
        # restore
        shutil.copy(FIXTURE, MANIFEST)
        r = (await admin.ask("manifest reload"))[0]
        check("reload_restored", r.startswith("return=Success"), r)
    print("manifest: %s" % ("passed" if not fails else "FAILED"))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
