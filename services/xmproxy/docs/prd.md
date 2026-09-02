# xmproxy as an IoT endpoint — Product Requirements

Status: draft v0.1, 2026-09-02. Owner: albert.david@gmail.com. Written from the
interview held on 2026-09-02; every decision below was confirmed with the owner
unless marked *proposed*.

## 1. Background

`xmproxysrv` is a headless XMPP chat bot (gloox based) that runs on a remote
Linux device and executes a fixed set of text commands sent by authorized XMPP
buddies. It was built for humans typing into a chat client. It works, is small,
supports BOSH, reconnects, and has an alias system that lets a deployment wire
commands to anything reachable from a shell (Domoticz, Tasmota, GPIO, ssh
tunnels).

The next step is to let a purpose-built Android app talk to the same daemon
without the user typing anything: fetch what the device can do, render buttons
and indicators, send commands, and receive state changes. The human chat mode
must keep working exactly as it does today.

## 2. Goal

Turn `xmproxysrv` into a robust, always-on IoT endpoint that an app can drive
machine-to-machine over XMPP, while remaining a chat bot for humans.

The owner named three outcomes that together define success:

1. **Tap-to-control.** From a phone, tap a control, the device acts, and the
   app shows a confirmation or a clear failure within a few seconds on a normal
   home connection.
2. **Rock-solid daemon.** The process runs for weeks without a manual restart,
   survives network flaps, server outages and account problems, and every
   command is either confirmed or clearly failed. It can fail over to a
   secondary XMPP account and return to the primary.
3. **Subscription-based notifications.** The device pushes state changes and
   alerts to buddies that subscribed, without the app polling.

## 3. Non-goals (for this project)

- The Android app itself. It will be built later in a separate session. This
  project delivers the protocol, the manifest, sample configs, and a scripted
  reference client good enough to prove the daemon end to end.
- XMPP server work. Extending the Snikket deployment wrapper with email-based
  account registration is a later, separate project.
- New action types on the device (native HTTP, MQTT). A control only ever maps
  to an existing text command or alias.
- End-to-end encryption (OMEMO), multi-user chat, file transfer.
- Changing the JSON-RPC-over-TCP side channel or the sysmgr service beyond what
  the buckets below require.

## 4. Users and roles

| Role | Who | May do |
|---|---|---|
| admin | device owner (admin buddy, backup admin buddy, any buddy given `admin` in the ACL file) | everything, including shell, reboot, config, ACL and manifest reload |
| operator | family members trusted to operate the home | run controls marked for operators, read all status, subscribe to events |
| viewer | guests | read status and manifest, subscribe to events, no actions |

Roles are assigned per bare JID in a separate ACL file. Roster members not
listed get a default role (*proposed: viewer*; confirm at bucket 3 checkpoint,
because today every roster member can run shell commands).

## 5. Functional requirements

### FR-1 Dual-mode messaging (additive)
- A message body whose first non-whitespace character is `{` or `[` is handled
  as JSON-RPC 2.0 (single request or batch). Anything else is handled exactly
  as today.
- Existing text commands, aliases, `help`, `echo`, response strings and the
  semicolon batch behavior are unchanged, byte for byte.

### FR-2 JSON-RPC 2.0 over XMPP
- Requests carry an `id`; responses echo it. Batches return a batch.
- A generic `exec` method runs any text command string and returns a structured
  result: the same return code and result text the chat mode would print, plus
  a task id when the command is asynchronous.
- Discovery methods: `describe` (device identity, daemon version, protocol
  version, JIDs the device may appear as), `list_commands` (command table with
  argument hints and minimum role), `get_manifest`.
- Errors use JSON-RPC error objects with a stable code set (parse error,
  invalid request, method not found, not authorized, command failed, busy).
- Async completion of a task is delivered as a JSON-RPC notification to the
  requester.
- Access checks apply per request using the sender's role, identically for
  chat and JSON paths.

### FR-3 Device-served manifest
- A JSON file on the device describes the UI: device name, groups, controls
  (button, toggle, indicator, text) with label, icon hint, the command or alias
  string to execute, whether the app should ask for confirmation, and the
  minimum role.
- Indicators declare the command that produces their value, a poll interval,
  and an optional regex capture to extract the value from the command output.
- The daemon validates the file at load and on reload, reports errors to the
  admin, and refuses to serve an invalid manifest rather than a partial one.
- Admin can reload the manifest over chat without restarting.
- Ships with two samples: a Domoticz home (curl aliases) and a bare Pi (gpio,
  uptime, ssh tunnel).

### FR-4 Subscriptions and notifications
- `subscribe` / `unsubscribe` methods with topics: an indicator id, `task`,
  `system` (connect, failover, reboot), or `*`.
- Subscriptions are persisted per JID and survive restarts.
- The daemon polls each indicator at its interval, compares with the last
  value, and sends a notification only on change (and once on subscribe so the
  app has an initial value).
- Notifications are fire-and-forget. An app that was offline re-reads state on
  connect. Rate limiting protects the XMPP server from flapping indicators.
- Existing gpio and sysmgr events keep flowing and are also exposed as topics.

### FR-5 Roles and ACL
- ACL file with one `jid role` line per buddy; loaded at start and reloadable.
- The command table's access column is enforced for every command in both
  modes. Admin-only: shell, reboot, poweroff, hostname, alias edit, ACL edit,
  manifest reload, buddy management, relay.
- Admin chat commands to list, add and remove ACL entries.
- Unauthorized attempts are answered with a clear error and logged.

### FR-6 Fallback account with return to primary
- Login file may define a fallback account (user, password, optional BOSH and
  TLS settings) which may live on a different server.
- After N consecutive failed connection attempts to the primary, the daemon
  connects with the fallback. While on the fallback it probes the primary at a
  slow interval and switches back as soon as the primary is reachable.
- `describe` and the manifest list both JIDs so the app knows where to look.
- Failover and failback are logged and published on the `system` topic.

### FR-7 Stability
- All access to the gloox client is serialized; sends, pings, disconnects and
  roster reads from other threads are safe.
- Reconnect uses exponential backoff with a cap and jitter.
- Keepalive ping timeout logic is correct and configurable.
- Clean shutdown on SIGTERM within a bounded time.
- Log lines carry a timestamp and level; debug noise is separable.

## 6. Non-functional requirements

- **NFR-1 Portability.** C++11, gloox and json-c only. Builds unchanged for
  the OpenWrt cross toolchains already in use. No new runtime dependencies on
  the device.
- **NFR-2 Primary target.** Raspberry Pi 4 on Raspberry Pi OS Lite, installed
  as a systemd service running as a non-root user. Docker on x86 remains
  supported.
- **NFR-3 Footprint.** Resident memory stays in the same order as today
  (roughly 10 MB without AI).
- **NFR-4 Testability.** A local test rig (Prosody in Docker plus a scripted
  XMPP test client) exercises chat and JSON paths without any cloud account.
  Every bucket adds tests that run from one command.
- **NFR-5 Compatibility.** Existing login, alias, botname and event files keep
  their format. New files are additive.
- **NFR-6 Security baseline.** No shell execution for non-admin roles.
  Secrets stay out of logs. `tlsverify: false` prints a loud warning.

## 7. Success criteria (measurable, checked at the final bucket)

| # | Criterion | Target |
|---|---|---|
| S1 | Tap-to-confirm latency, direct TCP mode, home LAN to cloud XMPP server | median under 2 s, 95th percentile under 5 s |
| S2 | Soak on Pi 4 with an indicator polling every 10 s and periodic network cuts | 72 h with zero restarts and zero lost confirmations |
| S3 | Failover | primary blocked: on fallback within 2 min; primary restored: back on primary within 10 min |
| S4 | Chat regression | scripted run of every existing text command produces byte-identical replies before and after |
| S5 | Unauthorized action from a viewer JID | rejected, logged, no side effect |
| S6 | Manifest error | malformed file is reported to admin and the previous manifest stays served |

## 8. Decision log

| Id | Decision | Status |
|---|---|---|
| D1 | Structured mode is JSON-RPC 2.0 in the XMPP message body, detected by a leading `{` or `[` | confirmed |
| D2 | UI definition is a device-served manifest file | confirmed |
| D3 | Controls map only to existing commands and aliases | confirmed |
| D4 | Roles: admin, operator, viewer, configured in a separate ACL file | confirmed |
| D5 | Indicators: device polls, pushes on change, app may also read on demand | confirmed |
| D6 | Fallback account with automatic return to primary | confirmed |
| D7 | C++11, gloox and json-c only; OpenWrt must keep building | confirmed |
| D8 | Pi 4 with Raspberry Pi OS Lite is the primary deployment target | confirmed |
| D9 | Android app is a later, separate session; this project ships a scripted reference client | confirmed |
| D10 | Test rig uses Prosody in Docker and a Python test client; Python is test-only and never runs on the device | *proposed*, verify at bucket 0 checkpoint |
| D11 | Default role for roster members absent from the ACL file is viewer | *proposed*, verify at bucket 3 checkpoint |
| D12 | Legacy GSM, SMS, USSD and firmware-update commands move behind a build option that defaults off | *proposed*, verify at bucket 7 checkpoint |
| D13 | Notifications are fire-and-forget; no device-side replay queue | *proposed*, verify at bucket 6 checkpoint |

## 9. Open questions

- Q1: Which XMPP server will the reference deployment use, Snikket (Prosody) only, or must ejabberd also be covered in tests?
- Q2: Should the fallback account be allowed to use the same server as the primary (account-level failover only), or is a different server the expected case?
- Q3: Maximum acceptable size for a single indicator value and for a manifest (XMPP servers cap stanza size, commonly 256 KB, some at 64 KB)?

## 10. Glossary

- **Buddy**: an XMPP contact in the daemon's roster.
- **Admin buddy**: the JID from the login file with full rights.
- **Manifest**: the device-served JSON document describing controls and indicators.
- **Topic**: a named event stream a buddy can subscribe to.
- **BOSH**: XMPP tunneled over HTTP, used behind restrictive firewalls.
