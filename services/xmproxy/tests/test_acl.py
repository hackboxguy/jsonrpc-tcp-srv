#!/usr/bin/env python
"""Bucket 3: roles and ACL enforcement (PRD S5, FR-5).

Uses the admin and guest accounts. The guest is made a roster member first
(acceptbuddy + subscribe), so it gets the default role viewer. The daemon
must run with --aclfile pointing at RUN_DIR/xmpp-acl.txt.
"""
import asyncio
import os
import sys
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
ACL_FILE = os.path.join(RUN, "xmpp-acl.txt")
BOT = "bot@%s" % DOMAIN
GUEST = "guest@%s" % DOMAIN

fails = []


def check(name, cond, detail=""):
    if cond:
        print("ok       %s" % name)
    else:
        print("FAIL     %s %s" % (name, detail))
        fails.append(name)


def first(replies):
    return replies[0] if replies else "<no reply>"


def denied(reply, role):
    return reply == "return=ActionBlocked : result=requires %s" % role


async def main():
    async with BotChat("admin@%s" % DOMAIN, ADMIN_PW, BOT, HOST, PORT) as admin, \
               BotChat(GUEST, GUEST_PW, BOT, HOST, PORT) as guest:
        # make sure the guest is a roster member (idempotent)
        await admin.ask("acceptbuddy %s" % GUEST)
        guest.send_presence(pto=BOT, ptype="subscribe")
        await asyncio.sleep(2.0)
        # start from a clean ACL
        await admin.ask("acl %s remove" % GUEST)

        # --- viewer (default role) ---
        r = first(await guest.ask("account"))
        check("viewer_reads_account", r.startswith("return=Success"), r)
        r = first(await guest.ask("hostname"))
        check("viewer_reads_hostname", r.startswith("return=Success"), r)
        r = first(await guest.ask("hostname newname"))
        check("viewer_cannot_set_hostname", denied(r, "admin"), r)
        r = first(await guest.ask("sleep 1"))
        check("viewer_cannot_sleep", denied(r, "operator"), r)
        r = first(await guest.ask("shellcmd ls"))
        check("viewer_cannot_shellcmd", denied(r, "admin"), r)
        r = first(await guest.ask("shellcmdresp"))
        check("viewer_cannot_read_shell_output", denied(r, "admin"), r)
        r = first(await guest.ask("alias"))
        check("viewer_lists_aliases", r.startswith("return=Success"), r)
        r = first(await guest.ask("alias t9=account"))
        check("viewer_cannot_edit_alias", denied(r, "admin"), r)
        r = first(await guest.ask("acl"))
        check("viewer_cannot_use_acl", denied(r, "admin"), r)
        r = first(await guest.ask("gpio 4"))
        check("viewer_reads_gpio", not r.startswith("return=ActionBlocked"), r)
        r = first(await guest.ask("gpio 4 1"))
        check("viewer_cannot_set_gpio", denied(r, "operator"), r)
        rs = await guest.ask("account;shellcmd ls", min_replies=2)
        check("batch_checked_per_command", len(rs) == 2 and rs[0].startswith("return=Success") and denied(rs[1], "admin"), repr(rs))
        h = first(await guest.ask("help"))
        check("viewer_help_hides_admin_commands", "shellcmd" not in h and "account" in h and "acl" not in h)

        # --- admin manages roles ---
        r = first(await admin.ask("acl"))
        check("admin_lists_acl", "default viewer" in r, r)
        r = first(await admin.ask("acl %s operator" % GUEST))
        check("admin_sets_operator", r.startswith("return=Success"), r)
        with open(ACL_FILE) as f:
            content = f.read()
        check("acl_file_persisted", "%s operator" % GUEST in content, content)
        r = first(await guest.ask("sleep 1"))
        check("operator_can_sleep", r.startswith("return=Success"), r)
        r = first(await guest.ask("gpio 4 1"))
        check("operator_can_set_gpio", not r.startswith("return=ActionBlocked"), r)
        r = first(await guest.ask("shellcmd ls"))
        check("operator_cannot_shellcmd", denied(r, "admin"), r)
        h = first(await guest.ask("help"))
        check("operator_help_shows_sleep", "sleep" in h and "shellcmd" not in h)

        r = first(await admin.ask("acl %s admin" % GUEST))
        check("admin_promotes_to_admin", r.startswith("return=Success"), r)
        r = first(await guest.ask("alias t9=account"))
        check("acl_admin_can_edit_alias", r.startswith("return=Success"), r)
        await guest.ask("alias t9=")  # cleanup

        r = first(await admin.ask("acl %s remove" % GUEST))
        check("admin_removes_entry", r.startswith("return=Success"), r)
        r = first(await guest.ask("sleep 1"))
        check("back_to_default_viewer", denied(r, "operator"), r)

        # --- reload from an externally edited file ---
        with open(ACL_FILE, "w") as f:
            f.write("# edited by test\n%s operator\n" % GUEST)
        r = first(await admin.ask("acl reload"))
        check("admin_reloads_file", r.startswith("return=Success"), r)
        r = first(await guest.ask("sleep 1"))
        check("reload_applied", r.startswith("return=Success"), r)
        await admin.ask("acl %s remove" % GUEST)

        # --- argument errors ---
        r = first(await admin.ask("acl nobody operator"))
        check("acl_rejects_non_jid", r.startswith("return=ArgError"), r)
        r = first(await admin.ask("acl %s boss" % GUEST))
        check("acl_rejects_unknown_role", r.startswith("return=ArgError"), r)
        r = first(await admin.ask("acl nobody@%s remove" % DOMAIN))
        check("acl_remove_unknown", r.startswith("return=ItemNotFound"), r)

        # admin buddy itself keeps admin even if listed lower
        await admin.ask("acl admin@%s viewer" % DOMAIN)
        r = first(await admin.ask("acl"))
        check("admin_buddy_always_admin", r.startswith("return=Success"), r)
        await admin.ask("acl admin@%s remove" % DOMAIN)

    try:
        with open(os.path.join(RUN, "xmproxysrv.log")) as f:
            log = f.read()
        check("denial_logged", "acl: %s (viewer) denied 'shellcmd ls', needs admin" % GUEST in log)
    except OSError:
        check("denial_logged", False, "no log")
    print("acl: %s" % ("passed" if not fails else "FAILED"))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
