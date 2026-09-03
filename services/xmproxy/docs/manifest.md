# Device manifest

Schema version 1. Status: implemented in bucket 5 (2026-09-03).

The manifest is a JSON file on the device (`--manifest=PATH`, on the Pi
`/etc/xmproxy/manifest.json` next to the login and ACL files). It describes
the controls an app should render. Every control maps to existing chat
commands or aliases, so the manifest is pure mapping: a Domoticz button is an
alias that runs `curl`, a GPIO toggle is two `gpio` commands.

The daemon validates the file at start and on `manifest reload`, refuses an
invalid file and keeps serving the previous one, and serves a normalized copy
to apps through the JSON-RPC method `get_manifest` (see `protocol.md`).

## File format

```json
{
  "manifest": 1,
  "device": {"name": "home", "description": "Domoticz on the living-room Pi", "icon": "home"},
  "groups": [
    {"id": "lights", "label": "Lights", "icon": "lightbulb",
     "controls": [
       {"id": "living", "type": "toggle", "label": "Living room",
        "on": "livingon", "off": "livingoff",
        "command": "livingstate", "interval": 60, "match": "\"Status\" : \"On\""},
       {"id": "gateopen", "type": "button", "label": "Open gate", "action": "gateopen",
        "confirm": true},
       {"id": "temp", "type": "indicator", "label": "Temperature",
        "command": "livingtemp", "interval": 120, "regex": "\"Temp\" : ([0-9.]+)", "unit": "°C"},
       {"id": "uptime", "type": "text", "label": "Uptime", "command": "uptime", "interval": 300}
     ]}
  ]
}
```

| Field | Required | Meaning |
|---|---|---|
| `manifest` | yes | schema version, must be `1` |
| `device.name` | yes | display name of the device |
| `device.description`, `device.icon` | no | free text and an icon hint for the app |
| `groups[]` | yes | ordered groups; `id` (letters, digits, `_`, `-`; unique), `label` (default: id), `icon` |
| `groups[].controls[]` | yes | ordered controls; `id` unique across the whole manifest |

Control fields:

| Field | Types | Meaning |
|---|---|---|
| `type` | all | `button`, `toggle`, `indicator`, `text` |
| `label`, `icon` | all | label (default: id) and icon hint |
| `action` | button | command string to run |
| `on`, `off` | toggle | command strings for the two positions |
| `command` | indicator, text, toggle (optional) | command string that reads the value or the toggle state |
| `interval` | indicator, text, toggle | seconds between reads, at least 5; drives the bucket 6 poller |
| `regex` | indicator, text | regular expression whose first capture group is the value; default: whole text |
| `match` | toggle | substring of the state text that means "on" |
| `unit` | indicator | unit shown next to the value |
| `confirm` | button, toggle | app asks the user before running; default `false` |
| `role` | all | minimum role to use the control: `admin`, `operator`, `viewer`. Default: `viewer` for indicator and text, `operator` for button and toggle |

Command strings (`action`, `on`, `off`, `command`) are interpreted exactly
like a chat message: lower-cased, aliases expanded. A literal `;` is rejected
at load; define an alias when a control needs several commands. A command
that does not resolve to a known command or alias is a warning, not an error,
so aliases may be defined later.

## Roles and the manifest grant

The control's `role` is an admin-approved grant. Whoever holds that role may
trigger the control even if the underlying command would need a higher role
when typed in chat: a family member with role `operator` can press "Open
gate" although the alias behind it runs `shellcmdtrig curl ...`, which is
admin-only as a typed command. The manifest file is owned by the device
admin, so listing an action there is the approval. Typed commands keep their
own rules: the same operator typing `shellcmd ...` is still denied.

The served manifest carries, per control, the effective `role` and an
`allowed` boolean evaluated for the caller, so an app can hide or grey out
what the user cannot use.

## Triggering controls

- JSON-RPC: `exec` with `{"control": "<id>"}`, plus `"arg": "on"` or `"off"`
  for a toggle. Without `arg`, a toggle with a `command` reads its state.
  The result has `control`, `return`, `result`, optional `task`, and
  `results` with one entry per executed step. Errors: `-32001` when the
  caller's role is below the control's, `-32602` for an unknown control or a
  bad toggle argument.
- Chat: `run <id>` and `run <id> on|off`. Denied: `ActionBlocked : requires
  <role>`. Unknown: `ItemNotFound`.
- Asynchronous actions (identify, shell commands) answer `InProgress` with a
  task number and finish with `task.done` as for `exec`.

## Managing the file

- `manifest` (any role): summary with device, file, groups, every control
  with type and role, and warnings.
- `manifest check` (admin): validate the file on disk without applying it.
- `manifest reload` (admin): apply the file; on error the reply carries the
  reason and the previous manifest stays served. Without `--manifest`,
  `get_manifest` returns error `-32004`.
- Validation errors name the group and control, for example
  `group 'lights' control 'living': toggle needs 'on' and 'off'`.

## Indicators and the poller (bucket 6)

`interval`, `regex` and `match` are declared here and used by bucket 6: the
daemon runs each indicator's command at its interval and pushes a
notification to subscribed buddies when the value changes. A command that
runs asynchronously (`shellcmd`) will have its captured output read through
`shellcmdresp` after completion, so Domoticz-style status reads work with
`shellcmd curl ...` aliases.

## Samples

- `helpers/configs/manifest-pi.json`: bare Pi with GPIO relay, door contact,
  identify and reboot.
- `helpers/configs/manifest-domoticz.json` with
  `helpers/configs/xmpp-alias-list-domoticz.txt`: lights, temperature and a
  gate through Domoticz's HTTP API.
- `tests/fixtures/manifest.json`: the rig manifest used by `test_manifest.py`.
