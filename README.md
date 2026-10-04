# jsonrpc-tcp-srv
A lightweight client/server implementation in C++ using json-rpc request/responses over raw-tcp port.

## JSON-RPC protocol notes (for client authors)

The services speak JSON-RPC 2.0 over plain TCP, with these limits:

- **Framing.** A connection carries a stream of JSON objects. They may be sent
  back to back, or separated by whitespace, newlines, `,` or NUL bytes
  (older clients send a NUL after each request). A request may arrive in
  any number of TCP segments.
- **Requests are capped at 256 KB.** Bytes outside a JSON object (other than
  the separators above) or a larger request are a framing error: the server
  answers once with `-32700` (`"id": null`) and closes the connection.
  Responses to earlier requests on that connection that are still queued
  may be lost.
- **`id` must be an integer** (a numeric string is accepted and converted).
  A request without `id` (a JSON-RPC notification) is answered with
  `-32600` Invalid Request. **Batches** (JSON arrays) are rejected with
  `-32600`.
- **Ordering.** Requests are handled by a single RPC thread per service, in
  arrival order. A slow method delays all clients. Long work is done
  asynchronously: the method answers with a `taskId`, and the client polls
  `get_rpc_req_status` with it.
- **Flow control.** At most 256 requests per connection wait for their
  responses; beyond that the server stops reading from the connection until
  half are answered. A client that does not read its responses (a send
  stalls for 1 s, or for more than 5 s in total within a minute) is
  disconnected.
- **Events.** `event_subscribe` registers a callback port. A subscriber is
  only removed after its deliveries failed continuously for 60 s, but
  events that fail are not retried. Subscribers should subscribe again after
  their own restart (a duplicate subscription is harmless).
- **Network exposure.** The services listen on all interfaces by default and
  have no authentication; some methods run shell commands or shut the
  service down. Restrict access with a firewall, or bind to `127.0.0.1`
  (`ADJsonRpcMgr::Start(port, log, emulation, "127.0.0.1")`).
