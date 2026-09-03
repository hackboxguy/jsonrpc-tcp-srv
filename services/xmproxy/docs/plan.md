# xmproxy IoT endpoint — Delivery plan

Companion to [prd.md](prd.md). Work is split into small buckets. Each bucket
is planned in detail right before it starts, built, then stopped at a
checkpoint where the owner reviews the output and confirms the decisions
listed for that bucket. No bucket starts before the previous checkpoint is
signed off.

Rules that apply to every bucket:

- Existing text commands and their replies do not change (S4 in the PRD).
- C++11, gloox, json-c only. OpenWrt cross build must still pass.
- Every bucket adds or extends automated tests runnable with one command.
- Every bucket ends with a short written summary of what changed, what was
  tested and how, and which decisions the owner must confirm.

## Bucket overview

| # | Bucket | Why it is in this position | Size |
|---|---|---|---|
| 0 | Baseline and test rig | Nothing later can be verified without it | S |
| 1 | Stability core: thread safety, backoff, shutdown, logging | The app will drive far more traffic than a human; fix the foundation first | M |
| 2 | Fallback account with return to primary | Owner's explicit robustness wish; builds on the new connection state machine | M |
| 3 | Roles and ACL enforcement | Must exist before the structured mode widens the attack surface | S |
| 4 | JSON-RPC 2.0 over XMPP | The machine-to-machine core | M |
| 5 | Device-served manifest | What the app renders | M |
| 6 | Subscriptions, indicator polling, notifications | Push side of the product | M |
| 7 | Legacy retirement and hardening | Safer after behavior is pinned by tests | S |
| 8 | Pi 4 packaging, soak test, release | Proves the success criteria on the real target | M |

Sizes: S about one session, M two to three sessions.

## Bucket 0 — Baseline and test rig

Scope
- Reproducible local build script for this machine (gloox 1.0.28 and json-c
  are installed) and a documented OpenWrt cross-build check.
- Test rig: Prosody in Docker with three pre-created accounts (bot, admin,
  guest) on a throwaway domain; a Python test client (slixmpp) that logs in,
  sends a message, and asserts on the reply; a runner script that starts the
  rig, starts the daemon with a test login file, runs the tests and tears down.
- Golden chat regression: send every enabled text command that is safe to run
  (no reboot, poweroff, shell) and record the replies as golden files. This
  pins current behavior for S4.
- Fix the copy-pasted xmproxy TCP test request file so the existing
  tcp-json-rpc-client test is meaningful.

Deliverables
- `services/xmproxy/tests/` with rig compose file, test client, golden files,
  and a single `run-tests.sh`.
- `services/xmproxy/docs/dev-setup.md`.

Acceptance
- `run-tests.sh` passes on a clean checkout on this machine.
- Golden files exist for every safe text command.

Decisions to verify at checkpoint
- D10: Python test client is acceptable as a development-only dependency. (confirmed 2026-09-02)
- Q1: Prosody-only test coverage is enough. (confirmed 2026-09-02)

Status: done 2026-09-02 on branch `m2m-extension`. Findings recorded in the
findings log below and folded into bucket 1.

## Bucket 1 — Stability core

Scope
- F1 from the findings log: heap corruption in the shared JSON-RPC library's
  response thread (seen as a sysmgr segfault). Find and fix in
  `lib/lib-jsonrpc-tcp` since xmproxysrv uses the same code.
- F2 from the findings log: re-subscribe to sysmgr, gpio and sms events when
  the peer service restarts, so async completions are not lost.
- Serialize all gloox client access behind one lock or an outbound queue
  drained by the connection thread. Make the client pointer lifecycle safe.
- Protect the command queue, inbox, session map and async task list.
- Reconnect with exponential backoff, cap and jitter; configurable.
- Correct keepalive ping accounting and timeout.
- Bounded, clean shutdown on SIGTERM (no second Ctrl-C).
- Log lines with timestamp and level; a `--loglevel` option.
- Stress test in the rig: 1000 rapid commands from two senders while the rig
  cuts the network several times.

- P1 (scheduled): bounded command queue with a Busy reply when full, AI
  prompts moved off the XMPP receive thread, sleep capped at 30 s, async
  commands time out with a Timeout reply when the completion never arrives.

Acceptance
- Stress test passes with no lost or duplicated replies.
- Golden chat regression unchanged.
- No data race reported by ThreadSanitizer on the x86 build.

Decisions to verify at checkpoint
- BOSH mode keeps its blocking receive with locked sends, documented as a
  known limitation; TCP mode uses timed receive with an outbound queue.
- Defaults: keepalive ping every 90 s, 3 misses; reconnect 2 s doubling to
  60 s plus jitter; async timeout 300 s; queue depth 64; sleep cap 30 s.
- Raspberry Pi install (bucket 8) must build gloox from source with OpenSSL,
  because distribution packages link GnuTLS and cannot authenticate against
  Snikket (finding F5).

Status: done 2026-09-02 on branch `m2m-extension`. See the checkpoint
summary in the commit message and findings F5 to F7 below.

## Bucket 2 — Fallback account

Scope
- Login file keys for a fallback account and its optional BOSH and TLS
  settings.
- Connection state machine: primary, primary-failing (count), on-fallback,
  probing-primary, switching-back.
- Background probe of the primary while on fallback; graceful switch back.
- `describe` groundwork: a place to report both JIDs.
- Rig test: block the primary at the network level, assert fallback within the
  target, unblock, assert return within the target.

Acceptance
- S3 targets met in the rig.

Decisions to verify at checkpoint
- Q2: fallback on the same server versus a different server. Implemented so
  that both work: the fallback has its own optional `fallbackserver` and
  `fallbackport`; without them the JID domain decides.
- Failure threshold N and probe interval defaults: `fallbackafter` 3,
  `primaryprobe` 300 s (rig uses 5 and 10 s).
- When the fallback also fails N times the daemon alternates back to the
  primary, so a device never gets stuck on a dead fallback.
- The probe uses its own throwaway gloox client on a separate thread, so the
  fallback session stays up until the primary is confirmed reachable.

Status: done 2026-09-02 on branch `m2m-extension`. Rig result: on the
fallback 19.5 s after the primary went down, back on the primary 13.1 s after
it returned, recovery after both servers were down 13.1 s after the primary
returned. `account` now reports the account in use.

## Bucket 3 — Roles and ACL

Scope
- ACL file format and loader; admin chat commands to list, add, remove, reload.
- Assign a minimum role to every entry in the command table; enforce it in the
  dispatcher for chat mode (the JSON path reuses the same check in bucket 4).
- Clear denial reply and log line.
- Rig tests with the guest account attempting admin commands.

Acceptance
- S5 holds for every admin command.
- Golden regression unchanged for the admin account.

Decisions to verify at checkpoint
- D11: default role for unlisted roster members is viewer. (confirmed 2026-09-02)
- The role assigned to each command, as implemented:
  - viewer: echo, help, version, uptime, hostname (read), publicip, localip,
    account, botname (read), buddylist, acceptbuddylist, alias (list),
    gpio (read), eventgpio, eventgsm, log commands
  - operator: gpio (set), sleep, identify, sonoff, dispclear, display,
    dispbklt, and the legacy GSM and SMS commands
  - admin: shellcmd, shellcmdtrig, shellcmdresp, reboot, poweroff, sysupdate,
    hostname (set), resethostname, botname (set), alias (edit), xmpshutdown,
    acceptbuddy, rejectbuddy, subscribe, unsubscribe, relaymsg, acl
- Unknown JIDs (not admin, not in the roster) stay silent on purpose; a bot
  that answers strangers advertises itself. Denials with an explicit
  `ActionBlocked : requires <role>` reply go to roster members only (F3
  resolved this way; the JSON path in bucket 4 behaves the same).
- `help` lists only the commands the sender may run.
- Without `--aclfile` the roles still work (admin buddies plus default role)
  and `acl` changes are kept in memory with a warning.

Status: done 2026-09-03 on branch `m2m-extension`. `test_acl.py` covers 30
checks: viewer reads, viewer denials, per-command checks inside a semicolon
batch, role changes taking effect immediately, persistence, external edit
plus reload, argument errors, admin buddy immune to demotion, denial logged.

## Bucket 4 — JSON-RPC 2.0 over XMPP

Scope
- Detector and parser for JSON bodies; single and batch.
- Methods: `ping`, `describe`, `list_commands`, `exec`.
- Error code set and response builder.
- Async task completion as a notification to the requester.
- Role check reused from bucket 3.
- Protocol spec in `docs/protocol.md`.
- Test client gains a JSON mode; tests cover every method, batch, malformed
  input, and unauthorized calls.

Acceptance
- Every method round-trips in the rig; malformed input never crashes or hangs
  the daemon.

Decisions to verify at checkpoint
- Method names and error codes in `protocol.md`. (confirmed 2026-09-03)
- `exec` runs a single command; a literal `;` is rejected and JSON batches
  group requests. An alias that expands to several commands still runs them
  all, because the alias is one named action, and the reply lists every
  step. (confirmed 2026-09-03)
- Command outcomes stay in the result with the same codes as chat, so
  `UnknownCmd` or `Fail` are results, not JSON-RPC errors. Only protocol
  problems (parse, invalid request, unknown method, bad params, not
  authorized, busy) are errors.
- Authorization is checked for every step of an alias batch before anything
  runs; a denied batch executes nothing (stricter than chat, which runs the
  allowed steps and denies the rest one by one).
- P3 duplicate suppression: the stored reply is replayed for a repeated id
  within 60 s and the command is not run again.

Status: done 2026-09-03 on branch `m2m-extension`. `test_jsonrpc.py` covers
31 checks; `tests/xmrpc.py` is a shell client for manual use.

## Bucket 5 — Manifest

Scope
- Manifest schema in `docs/manifest.md`; loader with validation; `get_manifest`
  method; admin `manifest reload` command; error reporting to admin.
- Two sample manifests (Domoticz home, bare Pi).
- Tests: valid, invalid, reload keeps the previous manifest on error.

Acceptance
- S6 holds. The reference client can print a text rendering of the manifest.

Decisions to verify at checkpoint
- Control types and fields in the schema: `button`, `toggle`, `indicator`,
  `text` with the fields listed in `docs/manifest.md`. (implemented
  2026-09-03, confirm at checkpoint)
- Manifest file location on the Pi: `/etc/xmproxy/manifest.json` next to
  the login and ACL files, passed with `--manifest`.
- The manifest grant: a control's `role` lets that role trigger the
  control's action even when the underlying command is admin-only when
  typed (for example a Domoticz alias that runs `shellcmdtrig curl`). The
  file is admin-owned, so listing an action is the approval. Typed commands
  keep their own rules. Without this, family members could never use a
  Domoticz button.
- `exec {"control": id}` and chat `run <id>` trigger controls by id; a
  toggle takes `on` or `off`, or reads its state without an argument.
- Command strings that do not resolve are warnings, not errors, so aliases
  may be defined after the manifest.

Status: done 2026-09-03 on branch `m2m-extension`. `test_manifest.py`
covers 37 checks including S6 (invalid file rejected with the reason, the
previous manifest stays served) and ten validation messages.

## Bucket 6 — Subscriptions and notifications

Scope
- `subscribe` / `unsubscribe`, persistence per JID, topics.
- Indicator poller: per-interval execution, change detection, initial value on
  subscribe, rate limiting.
- Bridge existing gpio and sysmgr events to topics.
- Tests: change detection, no notification without change, rate limit, restart
  keeps subscriptions.

Acceptance
- Reference client shows an indicator updating live while its underlying
  command output changes in the rig.

Decisions to verify at checkpoint
- D13: fire-and-forget delivery. (confirmed 2026-09-03) Implemented with a
  per-subscriber limit of 60 events per minute.
- Q3: value and manifest size limits. (confirmed 2026-09-03) One XMPP
  message per event or reply; keep indicator output small, shell output is
  capped at 1300 bytes by the daemon.
- P4 heartbeat topic: `heartbeat` login key, default 300 s, 0 disables.
- A control is polled only while somebody subscribes to it; the current
  value is delivered once on subscribe.
- Topics: control ids, `system`, `task`, `heartbeat`, `*`. Subscribing to a
  control requires the control's role.

Status: done 2026-09-03 on branch `m2m-extension`. `test_events.py` covers
initial value, change detection, silence without change, unsubscribe,
persistence, viewer subscriptions, toggle state, asynchronous shell
indicator, invalid topics, chat watch/unwatch, `*`, task and heartbeat
topics, and the `online` system event after a server restart.

## Bucket 7 — Legacy retirement and hardening

Scope
- Move GSM, SMS, USSD and firmware-update commands behind a CMake option that
  defaults off; delete dead SMS RPC scaffolding.
- Replace fixed char buffers and unchecked strcpy/sprintf in xmproxy sources.
- Remove duplicated CMake link branches.
- Update CLAUDE.md and README to match the code.

Acceptance
- Golden regression unchanged with the option off (legacy commands absent from
  help, which is the intended change) and on.

Decisions to verify at checkpoint
- D12: option default off; whether to delete instead. (confirmed
  2026-09-03: build option `WITH_LEGACY_GSM`, default OFF.) With it off the
  GSM, SMS, USSD and `sysupdate` commands and their handlers are not compiled,
  `--usbgsm` is accepted and ignored, and the SMS service is not probed for
  events. `help` and `list_commands` no longer show `sysupdate`.
- Kept as they were: the log commands (disabled in the table since before
  this work) and the `EXMPP_CMD_*` enum values, so the table order is stable.

Status: done 2026-09-03 on branch `m2m-extension`. Also: the command-line
parser uses std::string instead of fixed buffers, remaining `sprintf` calls
in xmproxy are bounded, dead SMS RPC scaffolding is gone, sysmgr's async
work packets are deleted through their own type, the duplicated CMake link
branches are one block, and CLAUDE.md matches the code.

## Bucket 8 — Pi 4 packaging, soak and release

Scope
- Install script and systemd unit for Raspberry Pi OS Lite, non-root user,
  proper file locations, log rotation.
- 72 h soak with an indicator polling every 10 s and scripted network cuts;
  latency measurement for S1.
- Release notes, version tag, updated Docker image.
- Owner's addition (2026-09-03): packaging for the `misc-tools` image
  builder (a `packages/` recipe and the `micropanel-touch` board config) so
  that the daemons ship in the owner's A/B Pi image, and a settings page in
  the `micropanel-touch` launcher for configuring xmproxy.

Decisions taken so far
- The soak runs on the development machine against the owner's Snikket
  server, using the Docker image (gloox built with OpenSSL) so that network
  cuts can be scripted with `docker network disconnect`. No Pi is reachable
  from this session.
- Layout on the Pi: binaries and libraries under `/opt/xmproxy`, config
  under `/etc/xmproxy`, mutable state under `/var/lib/xmproxy`; xmproxysrv
  runs as the `xmproxy` user, sysmgr as root (it executes admin shell
  commands and reboots). Logs go to journald.

Progress (2026-09-03)
- Docker image rebuilt from the branch (31 MB, gloox with OpenSSL); the repo
  gained `.dockerignore` and a git-less version fallback so the image builds
  from a clean context.
- Soak started 2026-09-03 10:12 UTC against the owner's Snikket server: the
  container polls three indicators (one through `shellcmd`, every 10 s),
  publishes a heartbeat every 60 s, and the host-side client probes `exec
  account` every minute; the chaos script cuts the container's network for
  90 s every 4 h. `tests/local/soak/report.py` summarizes at any time.
- `deploy/`: `build-from-source.sh` (gloox with OpenSSL, digest pinned),
  `install-pi.sh`, systemd units, `xmproxy.env`, `sysusers-xmproxy.conf`,
  `xmproxy-seed.sh` + `xmproxy-seed.service`.
- `misc-tools` branch `xmproxy-package`: hook, board config, skeleton,
  assertions, persistence and capability rows; a real image build was
  started to measure the size limits.

- Image build (2026-09-03): the `misc-tools` build of
  `micropanel-touch`/`luckfox-ctp` 00.52 with the xmproxy hook succeeded on
  the second attempt (first attempt: hook-list variables are expanded from
  the builder environment, now literal; a bare `-lgloox` link that only
  worked with a system gloox, now pkg-config flags). Slimmed rootfs 768 MiB
  against the 960 MiB ceiling, apps extension 1280 MB sufficient. Payload
  streamed to the owner's Pi 4 (00.51, slot A) through
  `ab-system-update stdin`; the Pi rebooted into slot B, committed 00.52,
  `xmproxy-seed`, `sysmgr` and `xmproxysrv` enabled and running, account and
  `/data/xmproxy` tree as designed, binary linked against the OpenSSL gloox.
  The daemon waits for real credentials in `/data/xmproxy/etc/xmpp-login.txt`.

Acceptance
- S1, S2 and S3 measured and recorded in `docs/soak-report.md`.

## Bucket 9 — IOT-Agent settings page in the micropanel-touch launcher

Requested 2026-09-03. Repo: `micropanel-touch` (LVGL, C++17), committed to
`main`.

Scope
- Network menu gains an "IOT-Agent" entry.
- Screen: username (bot JID), server, password with an eye button that
  toggles masking, a Connect button, a red/green indicator of the daemon's
  connection state.
- Saving writes `/data/xmproxy/etc/xmpp-login.txt` through a typed
  operation of the root broker (the UI is unprivileged) and restarts
  `xmproxysrv.service`; the file format stays the daemon's key-value login
  file so the chat, fallback and tuning keys keep working.
- Status: the UI polls the daemon's local JSON-RPC port
  (`get_online_status` on 127.0.0.1:40005) while the screen is shown.
- Later (owner): BOSH settings and a custom server port on the same screen.
- Tests for the settings class and the broker operation, docs updated
  (`docs/action-execution-contract.md`, misc-tools capability matrix).

Acceptance
- Entering credentials on the Pi's panel and pressing Connect makes the
  daemon log in; the indicator turns green; the file survives reboot and
  A/B update; the unit tests pass on the host.

Progress (2026-09-03)
- Host side delivered in micropanel-touch `a45f982` (main): Network tile
  "IOT-Agent" (icon: envelope), screen with Account / Server (optional) /
  Password (eye reveal, cleared on submit and on leaving the screen),
  Connect, and a disc indicator: green `Connected`, red `Not connected`
  (daemon up, no XMPP session) or `Agent not running` (RPC port closed),
  grey `Checking...`.
- Privilege path: typed broker operation `iot_agent_config` (user, optional
  server, password) validated by one shared validator (bare JID with
  host-name domain, host-name server, password 1–128 chars without
  whitespace/control characters because the login parser splits on
  whitespace); handler `micropanel-touch-iot-agent-config` gets the password
  on stdin, rewrites only `user:`/`pw:`/`server:` in
  `/data/xmproxy/etc/xmpp-login.txt` (adminbuddy, tuning and fallback lines
  preserved), atomic replace as root:xmproxy 0640, then
  `systemctl restart --no-block xmproxysrv.service`.
- Status poller `IotAgentStatusMonitor`: background thread, polls
  `get_online_status` on 127.0.0.1:40005 every 1.5 s only while the screen
  renews a 5 s lease, so an idle panel never touches the daemon.
- Panel-side memory: `/data/micropanel-touch/iot-agent.conf` holds account
  and server, never the password.
- Tests added (61/61 pass on the host): validator, broker wire shapes,
  client round trip, settings file, monitor against a fake agent, handler
  contract + policy, starter config (6 Network tiles), headless navigation.
- misc-tools `e27275b` (main): capability row `xmproxy-configuration` is
  `supported`; PERSISTENCE.md names the handler and the launcher file.
- Pi verification (2026-09-03, image 00.53 built from micropanel-touch
  `a45f982` + jsonrpc-tcp-srv `8d0de2c`, applied over A/B update): the
  broker request `iot_agent_config` sent as the HMI user rewrote
  `/data/xmproxy/etc/xmpp-login.txt` (root:xmproxy 0640, `adminbuddy:`
  preserved, `server:` added), a malformed request was refused without
  touching the file, and the restarted daemon reported `online` on
  127.0.0.1:40005 three seconds later against a LAN Prosody
  (`bot@localhost`, server `192.168.1.80`). The green indicator itself was
  checked in a headless render on the host; the Pi has no control socket,
  so tapping the real panel is the owner's check.
- Deferred, as agreed: BOSH toggle and custom port on the same screen. The
  admin buddy is not on the screen; it stays whatever the seeded login file
  or a hand edit says (the ACL file is the intended way to grant roles).

## Proposed additions (not yet scheduled, owner to decide)

Raised at the bucket 0 checkpoint. Each names the bucket it would join.

| Id | Proposal | Would join |
|---|---|---|
| P1 | Per-command execution timeout and a bounded command queue, so one hung command cannot block every other sender. | bucket 1 (scheduled 2026-09-02) |
| P2 | Admin `status` chat command and `describe` fields: connection state, active account (primary or fallback), uptime, queue depth, last error. | bucket 1 and 2 |
| P3 | Duplicate suppression in the JSON path: remember (sender, request id) for a short window so a stanza resent by the server after a reconnect does not run a control twice. | bucket 4 (scheduled 2026-09-02) |
| P4 | `system.heartbeat` topic at a slow interval so the app can show "device alive" and detect a silent death. | bucket 6 (scheduled 2026-09-02) |
| P5 | `xmproxysrv --check-config` that validates login, ACL and manifest files and exits, for use before `systemctl restart`. | bucket 5 |
| P6 | Warn loudly when the login file is world readable or `tlsverify: false` is set. | bucket 8 |
| P7 | Run the rig against the Snikket server image instead of Prosody 0.11.9 once the server wrapper work starts, for fidelity with the real deployment. | bucket 8 |
| P8 | Optional bucket 9: client-certificate device authentication. Device holds an X.509 client certificate and logs in with SASL EXTERNAL (XEP-0178) instead of a password; gloox supports client certificates and the EXTERNAL mechanism. Server side needs Prosody's client-certificate modules, which Snikket does not enable by default, so this pairs with the Snikket wrapper project. Benefits: no password on the device, revocation by certificate, and a path to per-device identity. | new bucket 9 |

## Findings log

Problems discovered while building the rig, with the bucket that owns them.

| Id | Found | Finding | Owner |
|---|---|---|---|
| F1 | bucket 0 | `sysmgr` segfaulted in `malloc` inside `json_object_new_int64`, called from `ADJsonRpcProxy::json_send_result_response_string` on the RPC response thread, while several clients were talking to the services at once. Heap corruption in `lib/lib-jsonrpc-tcp`, which xmproxysrv shares. Core dump was captured on the dev machine. | bucket 1 |
| F2 | bucket 0 | xmproxysrv subscribes to sysmgr/gpio/sms events once at startup. If the peer restarts, completions of async commands (identify, shellcmd) never arrive and the chat reply stays at InProgress. Restarting xmproxysrv fixes it, so re-subscription is missing. | bucket 1 |
| F3 | bucket 0 | An unknown JID gets no reply at all, not even a denial. Fine for humans, but an app needs an explicit error to show. | bucket 3 |
| F4 | bucket 0 | `utils/tests/xmproxy-test/requests.txt` contained sysmgr requests; replaced with real xmproxy RPCs. | fixed in bucket 0 |
| F1 root cause | bucket 1 | The event manager in `lib/lib-jsonrpc-tcp` pushed and popped two std::deque queues and a subscriber vector from different threads with no lock; the same library also freed `new[]` buffers with `free`/`delete`, released a json-c child object twice (assertion under json-c 0.19) and read a ready flag unsynchronized. All fixed; ASan and TSan stress runs are clean. | fixed in bucket 1 |
| F2 | bucket 1 | Event subscriptions are now re-attempted every 30 s; a restarted peer is re-subscribed and async completions resume (verified by `test_resilience.py`). | fixed in bucket 1 |
| F5 | bucket 1 | gloox 1.0.28 linked against GnuTLS (Arch, Debian and Raspberry Pi OS packages) cannot authenticate against Snikket: SCRAM-SHA-1-PLUS proposes a channel-binding type the server rejects and plain SCRAM-SHA-1 is refused as malformed. The same daemon linked against an OpenSSL-built gloox authenticates in 0.5 s. The Docker image already builds gloox with OpenSSL; the Pi install must do the same. | bucket 8 |
| F6 | bucket 1 | Under AddressSanitizer the GnuTLS-linked gloox crashes in `gnutls_x509_trust_list_deinit` during disconnect on a plaintext session. Not reproduced with the OpenSSL build, which is the supported configuration; noted for anyone running the sanitizer rig. | documented |
| F7 | bucket 1 | Shutdown ordering: the event receiver was destroyed before the RPC manager's threads stopped. `main.cpp` now declares it first and subscribes only once the RPC server listens. | fixed in bucket 1 |
| F8 | bucket 5 | ThreadSanitizer caught a shutdown race: the timer thread could still deliver a heartbeat (async-task sweep) while the manager was being destroyed. `stop_timer()` now disarms the timer and joins its thread, `main` stops the timer before the manager, thread stops join cooperatively before cancelling, and the manager ignores heartbeats once stopping. | fixed in bucket 5 |

## Checkpoint template

Each bucket ends with a message to the owner containing:

1. What changed (files, behaviors).
2. What was tested, the command to rerun it, and the result.
3. Decisions to confirm before the next bucket, each with the recommended
   answer.
4. Anything deliberately left out and why.
