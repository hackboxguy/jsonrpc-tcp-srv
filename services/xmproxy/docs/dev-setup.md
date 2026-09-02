# xmproxy development setup

How to build `xmproxysrv` on a Linux workstation and run the automated tests
against a throwaway local XMPP server. Nothing here runs on a device.

## Prerequisites

| What | Why | Checked with |
|---|---|---|
| cmake 3.14+, g++ with C++11, pkg-config | build | `cmake --version` |
| gloox 1.0.28 dev package | XMPP client library | `pkg-config --modversion gloox` |
| json-c dev package | JSON parsing | `pkg-config --modversion json-c` |
| Docker | Prosody test server | `docker ps` |
| Python 3.10+ with venv | test client (slixmpp) | `python3 --version` |

The build also needs Git for the version stamp and the Linux i2c headers for
the display library.

## Build

```bash
cd jsonrpc-tcp-srv
cmake -H. -BOutput -DCMAKE_INSTALL_PREFIX=$PWD/Output/install -DCMAKE_BUILD_TYPE=Release
cmake --build Output -- install -j$(nproc)
export LD_LIBRARY_PATH=$PWD/Output/install/lib
./Output/install/bin/xmproxysrv --help
```

`Output/` is ignored by git. The test runner does this build for you.

## Run the tests

```bash
services/xmproxy/tests/run-tests.sh
```

The runner:

1. builds into `Output/` (skip with `--no-build`),
2. starts Prosody in Docker on `127.0.0.1:5222` with accounts `bot`,
   `admin` and `guest` on the domain `localhost` (see `tests/rig/`),
3. starts `sysmgr` (emulation mode) and `xmproxysrv` with the fixtures in
   `tests/fixtures/`, runtime files in `tests/.run/`,
4. waits until the bot reports online on its TCP JSON-RPC port 40005,
5. runs five groups and reports each:
   - chat golden regression (`test_chat_regression.py`, cases in
     `cases.json`, expected replies in `golden/`),
   - guest access baseline (`test_guest_access.py`),
   - TCP JSON-RPC regression (`tcp-json-rpc-client` with
     `utils/tests/xmproxy-test/`),
   - stress (`test_stress.py`): admin and guest each send 250 commands while
     Prosody is restarted twice; passes only with zero lost or extra replies,
   - resilience (`test_resilience.py`): bot back online after a server
     restart, async completion after sysmgr is killed and restarted, and
     SIGTERM exit within 5 s. Runs last because it stops xmproxysrv.
6. checks the daemons are still alive, then stops everything.

`--quick` skips the stress and resilience groups (about 2 minutes saved).

`--keep` leaves the rig and daemons running so you can chat with the bot from
any XMPP client by logging in as `admin@localhost` (password in
`tests/rig/accounts.env`) against `127.0.0.1:5222` without TLS.

`--record` rewrites the golden files. Only do this after an intended change in
reply text, and review the diff of `golden/` before committing.

`--only NAME` runs a single chat case.

## What the golden regression covers

Every enabled text command that is safe to run unattended: echo, help,
account, botname, version, uptime, hostname, localip, buddy lists, alias add,
use, list and delete, an alias from the seed file, an unknown command,
a semicolon batch, sleep, commands whose backing service is absent (gpio,
display, sonoff), argument errors, relay to a non-buddy, and the asynchronous
identify command that answers twice.

Excluded on purpose: reboot, poweroff, shellcmd, shellcmdtrig, sysupdate,
publicip (needs internet), and buddy subscription commands that change roster
state. The guest test covers acceptbuddy and the subscription handshake.

Values that legitimately differ per host or run (version number, uptime,
host name, IP addresses, task ids) are normalized by regex rules declared per
case in `cases.json`.

## Test rig details

- Server: `prosody/prosody:latest` (0.11.9), config in `tests/rig/prosody.cfg.lua`.
  TLS and registration are disabled, plaintext SASL is allowed, offline
  storage is on. The container is recreated on every run, so roster state
  does not leak between runs.
- Client: `tests/xmppclient.py`, a small slixmpp wrapper. `ask()` sends one
  message and collects replies until the bot has been quiet for 1.5 s.
- Ports: Prosody 5222, sysmgr 40001, xmproxysrv 40005. All on 127.0.0.1.

## Runtime tuning keys (login file) and logging

Optional keys in `xmpp-login.txt`, all with safe defaults: `pinginterval`
(90 s), `pingmisses` (3), `reconnectmin` (2 s), `reconnectmax` (60 s),
`asynctimeout` (300 s). See the comments in `srv/xmpp-login.txt`.

Log lines carry a timestamp and level. `--loglevel=error|warn|info|debug`
selects the level; `--debuglog` still enables everything including gloox's
stanza dump.

## Sanitizer runs

`stress-scenario.sh` runs the chat regression and TCP bursts against both
daemons concurrently, for any install tree, and prints sanitizer reports
found in `.run/`:

```bash
# AddressSanitizer tree (use an OpenSSL-built gloox, see below)
cmake -H. -BOutput-asan -DCMAKE_INSTALL_PREFIX=$PWD/Output-asan/install -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" -DCMAKE_C_FLAGS="-fsanitize=address -g" \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address
cmake --build Output-asan -- install -j$(nproc)
ASAN_OPTIONS=detect_leaks=0:new_delete_type_mismatch=0 \
  services/xmproxy/tests/stress-scenario.sh --install $PWD/Output-asan/install --iterations 2

# ThreadSanitizer tree
cmake -H. -BOutput-tsan -DCMAKE_INSTALL_PREFIX=$PWD/Output-tsan/install -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" -DCMAKE_C_FLAGS="-fsanitize=thread -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=thread
cmake --build Output-tsan -- install -j$(nproc)
TSAN_OPTIONS=report_signal_unsafe=0 \
  services/xmproxy/tests/stress-scenario.sh --install $PWD/Output-tsan/install --iterations 2 --tcp-repeat 150
```

`new_delete_type_mismatch=0` mutes reports about work packets deleted through
an `unsigned char *` in the services' async handlers; they are plain-data
structs and harmless under C++11, and will be tidied in bucket 7.

## gloox must be built with OpenSSL for real servers

Distribution packages of gloox (Arch, Debian, Raspberry Pi OS) link GnuTLS.
That build cannot authenticate against Snikket or any Prosody with TLS 1.3
channel binding: SCRAM-SHA-1-PLUS proposes an unsupported binding type and
plain SCRAM-SHA-1 is rejected as malformed. The local Prosody rig does not
use TLS, so it hides the problem. For anything real, build gloox 1.0.28 with
`./configure --with-openssl --without-gnutls` (exactly what the Dockerfile
does), point `PKG_CONFIG_PATH` at its `lib/pkgconfig`, and put its `lib` on
`LD_LIBRARY_PATH`. The Pi install in bucket 8 will script this.

## Manual check against a real server

Put a login file with real credentials under `tests/local/` (gitignored),
start `xmproxysrv` with `--port=40015` so it does not collide with the rig,
and use `xmppclient.py` with `plaintext=False`:

```bash
.venv/bin/python -c "import asyncio,sys; sys.path.insert(0,'.'); from xmppclient import BotChat
async def m():
    async with BotChat('me@example.org','pw','bot@example.org','example.org',5222,plaintext=False) as c:
        print(await c.ask('account'))
asyncio.run(m())"
```

## Cross-compile check (OpenWrt)

The repo supports `-DCMAKE_TOOLCHAIN_FILE=...` (see the comment in the root
`CMakeLists.txt`). No toolchain is present on this workstation, so the check
is manual for now: configure with the toolchain file and confirm
`xmproxysrv` links. Bucket 7 will add a CI-style script once a toolchain
path is agreed.
