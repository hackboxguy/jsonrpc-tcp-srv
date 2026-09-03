# Soak report (bucket 8)

Status: **running**. Started 2026-09-03 10:12 UTC, planned duration 72 h.
Interim numbers below are refreshed with `tests/local/soak/report.py`; the
final section is filled in when the soak ends.

## Setup

- Device under test: the Docker image built from `m2m-extension` (Alpine,
  gloox 1.0.28 with OpenSSL, 31 MB), container `xmproxy-soak`, read-only
  root, `--restart unless-stopped`, on the development machine's Docker
  bridge network.
- Server: the owner's Snikket server (Prosody, TLS 1.3) over the internet.
- Bot account: `aibot@remotekit.duckdns.org`; admin buddy
  `test@remotekit.duckdns.org`; heartbeat every 60 s.
- Manifest: `uptime` (text, 10 s), `load` (indicator through
  `shellcmd cat /proc/loadavg`, 10 s, regex first field), `who` (300 s).
- Client (`soak-client.py`, slixmpp, on the host): subscribes to the three
  controls plus `system`, `task`, `heartbeat`; logs every event; every 60 s
  sends `exec account` and records the round trip (S1 probe).
- Chaos (`chaos.sh`): every 4 h the container is disconnected from its
  network for 90 s (`docker network disconnect`), forcing a real session loss
  and reconnect against the remote server.

## Interim readings

| Time since start | Probes answered / lost | Latency median / p95 / max | Heartbeats (gaps over 3 min) | Daemon restarts | Notes |
|---|---|---|---|---|---|
| 1.3 h | 76 / 0 | 0.140 s / 0.151 s / 0.186 s | 77 (0) | 0 | no warnings, no errors |
| 2.5 h | 147 / 0 | 0.140 s / 0.151 s / 0.186 s | 148 (0) | 0 | before the first network cut |

## Success criteria mapping

- S1 (tap-to-confirm latency, direct TCP): probe median under 2 s and p95
  under 5 s. Interim: 0.14 s / 0.15 s.
- S2 (72 h, indicator every 10 s, periodic network cuts, zero restarts,
  zero lost confirmations): pending.
- S3 (failover): measured in the rig by `test_failover.py` (19.5 s to the
  fallback, 13.1 s back); the soak has no fallback account configured
  because the owner provided one bot account.

## Final result

Pending.
