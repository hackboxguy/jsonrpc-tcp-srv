# xmproxy machine-to-machine protocol (JSON-RPC 2.0 over XMPP)

Protocol version 1. Status: implemented in bucket 4 (2026-09-03).

## Transport

- Plain XMPP chat messages between an authorized buddy (see roles in
  `dev-setup.md`) and the bot. Nothing else changes on the wire: an app uses
  a normal XMPP library and sends message bodies.
- A body whose first non-blank character is `{` or `[` is handled as
  JSON-RPC 2.0. Any other body is a chat command, exactly as before.
- Replies come back as message bodies to the sender. Unknown JIDs (not an
  admin buddy, not in the roster) are never answered, in either mode.
- One request per message body, or a batch (JSON array). Batches are served
  in order; the reply is an array with one entry per request that had an
  `id`. A request without `id` is a notification and gets no reply.
- Replies to a request are always sent as one message. Asynchronous command
  completions arrive later as notifications (below).

## Envelope

Request: `{"jsonrpc": "2.0", "id": 7, "method": "exec", "params": {"cmd": "uptime"}}`

`id` may be a number or a string. `jsonrpc` may be omitted; if present it
must be `"2.0"`.

Success: `{"jsonrpc": "2.0", "result": {...}, "id": 7}`

Error: `{"jsonrpc": "2.0", "error": {"code": -32001, "message": "Not authorized", "data": {...}}, "id": 7}`

## Methods

| Method | Minimum role | Params | Result |
|---|---|---|---|
| `ping` | viewer | none | `{"pong": true, "time": <unix seconds>}` |
| `describe` | viewer | none | device and session description, see below |
| `list_commands` | viewer | none | array of `{"name", "args", "role", "allowed"}` for every enabled chat command; `allowed` is evaluated for the caller |
| `exec` | per command | `{"cmd": "<one chat command>"}` | see below |

### describe

```json
{"name": "myhome-pi", "version": "00034", "protocol": 1,
 "jid": "bot@example.org", "primary": "bot@example.org",
 "fallback": "bot@backup.example.org", "on_fallback": false,
 "role": "operator",
 "methods": ["ping", "describe", "list_commands", "exec"],
 "notifications": ["task.done"]}
```

`fallback` is present only when one is configured. `role` is the caller's
role. The manifest (bucket 5) will be reachable through a `get_manifest`
method and listed in `methods`.

### exec

`cmd` is one command string, interpreted exactly like a chat message: it is
lower-cased, aliases are expanded (an alias may expand to several commands),
and the role of every resulting command is checked before anything runs.
A literal `;` in `cmd` is rejected with `-32602`; use a JSON batch to group
several commands.

Result:

```json
{"return": "Success", "result": "10 Hours",
 "results": [{"cmd": "uptime", "return": "Success", "result": "10 Hours"}]}
```

- `results` has one entry per executed command (usually one; more for an
  alias that expands to a batch).
- Top-level `return` is `Success` when every step succeeded, otherwise the
  first non-success code. Top-level `result` is the text of the last step.
- Return codes are the same strings chat prints after `return=`:
  `Success`, `Fail`, `InProgress`, `Busy`, `Timeout`, `ArgError`,
  `ActionBlocked`, `HostUnreachable`, `UnknownCmd`, and so on.
- An unknown command is not a JSON-RPC error; it comes back as
  `"return": "UnknownCmd"` so that chat and JSON agree.
- Asynchronous commands (for example `identify`, `shellcmd`) return
  `"return": "InProgress"` with `"task": <n>` (also inside the step). The
  completion arrives later as a `task.done` notification.

### task.done notification

Sent by the bot to the requester when an asynchronous command finishes or
times out (`asynctimeout`):

```json
{"jsonrpc": "2.0", "method": "task.done",
 "params": {"task": 12, "return": "Success", "id": 7}}
```

`id` echoes the id of the `exec` request that started the task.

## Error codes

| Code | Meaning |
|---|---|
| -32700 | Parse error: body is not valid JSON; `id` is null |
| -32600 | Invalid request: not an object, bad `jsonrpc`, missing `method`, empty batch |
| -32601 | Method not found |
| -32602 | Invalid params: `exec` without `cmd`, empty command, or a `;` batch |
| -32001 | Not authorized: `data` carries `cmd` and `requires` (the role needed). Nothing was executed. |
| -32002 | Busy: the command queue is full; retry later |

## Duplicate suppression

The bot remembers, per sender, the reply to each request `id` for 60 seconds
(up to 256 entries). A request that repeats an `id` inside that window gets
the stored reply again and is not executed a second time. Apps should use
fresh ids per action and may safely resend after a reconnect.

## Sizes and limits

- One XMPP message per reply; servers commonly cap stanzas at 256 KB
  (Snikket advertises `max-bytes` 262144). `list_commands` and `describe` are
  a few KB.
- The command queue holds 64 pending requests across all senders.

## Examples

Batch:

```json
[{"jsonrpc":"2.0","id":1,"method":"ping"},
 {"jsonrpc":"2.0","id":2,"method":"exec","params":{"cmd":"account"}}]
```

Reply:

```json
[{"jsonrpc":"2.0","result":{"pong":true,"time":1788480000},"id":1},
 {"jsonrpc":"2.0","result":{"return":"Success","result":"bot@example.org","results":[{"cmd":"account","return":"Success","result":"bot@example.org"}]},"id":2}]
```

Denied:

```json
{"jsonrpc":"2.0","error":{"code":-32001,"message":"Not authorized","data":{"cmd":"shellcmd ls","requires":"admin"}},"id":3}
```
