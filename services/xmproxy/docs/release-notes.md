# xmproxy m2m-extension release notes

Branch `m2m-extension`, 2026-09-03. Every item below is covered by the
automated suite in `services/xmproxy/tests` (`run-tests.sh`), which runs a
local two-server Prosody rig, a golden chat regression, stress, failover,
resilience, ACL, JSON-RPC, manifest and event tests, and passes under
AddressSanitizer and ThreadSanitizer.

## For people chatting with the bot

- Every existing text command answers exactly as before (pinned by 30
  golden cases). New commands: `acl`, `manifest`, `run`, `watch`, `unwatch`.
- Roles: admin, operator, viewer. Roster members are viewers unless the
  admin gives them a role (`acl <jid> operator`). Denied commands answer
  `return=ActionBlocked : result=requires <role>`; `help` only lists what
  you may run.
- `account` reports the account in use, which may be the fallback.

## For operators

- The daemon reconnects with backoff, keeps the session alive with pings,
  survives XMPP server restarts and peer service restarts, and exits within
  seconds on SIGTERM.
- Optional fallback XMPP account (`fallbackuser`, `fallbackpw`, ...) with
  automatic return to the primary.
- Login-file keys: `server`, `port`, `pinginterval`, `pingmisses`,
  `reconnectmin`, `reconnectmax`, `asynctimeout`, `fallbackafter`,
  `primaryprobe`, `heartbeat`. Options: `--aclfile`, `--manifest`,
  `--subscrfile`, `--loglevel`. Timestamped log lines.
- gloox must be built with OpenSSL for Snikket and other TLS 1.3 servers;
  the Docker image and `deploy/build-from-source.sh` do so.
- Build option `WITH_LEGACY_GSM` (default OFF) for the GSM, SMS, USSD and
  `sysupdate` commands of the original product.
- Bugs fixed in the shared library: unlocked event queues (heap corruption
  under load), mismatched allocations, a json-c double release, lost event
  subscriptions after a peer restart, shutdown races.

## For app developers

- JSON-RPC 2.0 in XMPP message bodies: `ping`, `describe`,
  `list_commands`, `exec`, `get_manifest`, `subscribe`, `unsubscribe`,
  `get_subscriptions`; `task.done` and `event` notifications; batches;
  duplicate suppression. Specification: `docs/protocol.md`.
- Device-served manifest with buttons, toggles, indicators and text,
  role grants per control, validation and reload. Specification:
  `docs/manifest.md`.
- Shell client: `tests/xmrpc.py`. Python reference client:
  `tests/xmppclient.py`.

## Deployment

- Raspberry Pi OS Lite: `services/xmproxy/deploy/install-pi.sh` (builds
  gloox with OpenSSL and the daemons, installs systemd units running
  xmproxysrv as the `xmproxy` user, config under `/etc/xmproxy`, state under
  `/var/lib/xmproxy`).
- Docker: `docker-compose.yml` / `start-xmpp-chatbot.sh` pass the ACL,
  manifest and subscription files from `/xmpp-data`.

## Soak

See `docs/soak-report.md` (filled in when the 72 h soak against the
owner's Snikket server completes).
