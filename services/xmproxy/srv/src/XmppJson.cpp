// JSON-RPC 2.0 over XMPP for xmproxysrv (bucket 4). See docs/protocol.md.
//
// A message body starting with '{' or '[' is handled here instead of the
// chat parser. Requests are served on the worker thread; replies go back to
// the sender as message bodies. Command outcomes are carried in the result
// (same codes as chat); only protocol-level problems are JSON-RPC errors.
#include "XmppMgr.h"
#include <json-c/json.h>
#include <sstream>

#define XMJSON_PROTOCOL_VERSION 1
#define XMJSON_ERR_PARSE -32700
#define XMJSON_ERR_INVALID_REQUEST -32600
#define XMJSON_ERR_METHOD_NOT_FOUND -32601
#define XMJSON_ERR_INVALID_PARAMS -32602
#define XMJSON_ERR_NOT_AUTHORIZED -32001
#define XMJSON_ERR_BUSY -32002
#define XMJSON_ERR_NO_MANIFEST -32004
#define XMJSON_RECENT_WINDOW_S 60
#define XMJSON_RECENT_MAX 256

extern XMPROXY_CMD_TABLE xmproxy_cmd_table[];
extern const int xmproxy_cmd_table_size;

namespace {
json_object *make_error(int code, const char *message, json_object *data) {
  json_object *err = json_object_new_object();
  json_object_object_add(err, "code", json_object_new_int(code));
  json_object_object_add(err, "message", json_object_new_string(message));
  if (data != NULL)
    json_object_object_add(err, "data", data);
  return err;
}
// response envelope; takes ownership of id (may be NULL -> null), result or
// error
json_object *make_response(json_object *id, json_object *result,
                           json_object *error) {
  json_object *resp = json_object_new_object();
  json_object_object_add(resp, "jsonrpc", json_object_new_string("2.0"));
  if (error != NULL)
    json_object_object_add(resp, "error", error);
  else
    json_object_object_add(resp, "result", result);
  json_object_object_add(resp, "id", id);
  return resp;
}
std::string id_key(json_object *id) {
  if (id == NULL)
    return "";
  return json_object_to_json_string_ext(id, JSON_C_TO_STRING_PLAIN);
}
} // namespace

std::string XmppMgr::json_busy_response() {
  json_object *resp =
      make_response(NULL, NULL, make_error(XMJSON_ERR_BUSY, "Busy", NULL));
  std::string out =
      json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN);
  json_object_put(resp);
  return out;
}

bool XmppMgr::find_recent_reply(const std::string &sender,
                                const std::string &id, std::string &response) {
  std::lock_guard<std::mutex> lock(recentMutex);
  time_t now = time(NULL);
  for (std::deque<RecentReply>::iterator it = recentReplies.begin();
       it != recentReplies.end(); ++it) {
    if (it->sender == sender && it->id == id &&
        now - it->when <= XMJSON_RECENT_WINDOW_S) {
      response = it->response;
      return true;
    }
  }
  return false;
}
void XmppMgr::remember_reply(const std::string &sender, const std::string &id,
                             const std::string &response) {
  std::lock_guard<std::mutex> lock(recentMutex);
  time_t now = time(NULL);
  while (!recentReplies.empty() &&
         (recentReplies.size() >= XMJSON_RECENT_MAX ||
          now - recentReplies.front().when > XMJSON_RECENT_WINDOW_S))
    recentReplies.pop_front();
  RecentReply r;
  r.sender = sender;
  r.id = id;
  r.response = response;
  r.when = now;
  recentReplies.push_back(r);
}

void XmppMgr::send_task_notification(const std::string &to, int taskID,
                                     const std::string &result,
                                     const std::string &reqId) {
  json_object *params = json_object_new_object();
  json_object_object_add(params, "task", json_object_new_int(taskID));
  json_object_object_add(params, "return",
                         json_object_new_string(result.c_str()));
  if (!reqId.empty()) {
    json_object *idObj = json_tokener_parse(reqId.c_str());
    if (idObj != NULL)
      json_object_object_add(params, "id", idObj);
  }
  json_object *note = json_object_new_object();
  json_object_object_add(note, "jsonrpc", json_object_new_string("2.0"));
  json_object_object_add(note, "method", json_object_new_string("task.done"));
  json_object_object_add(note, "params", params);
  XmppProxy.send_reply(
      json_object_to_json_string_ext(note, JSON_C_TO_STRING_PLAIN), to);
  json_object_put(note);
}

// serve one request object; returns the response object or NULL for a
// notification (no id)
static json_object *serve_one(XmppMgr *mgr, json_object *req,
                              const std::string &sender, XM_ROLE role);

namespace {
struct ExecOutcome {
  json_object *result; // owned by caller
  json_object *error;  // owned by caller
};
} // namespace

// exec: params {"cmd": "<text command>"}; one command string, aliases expand
// exactly as in chat, literal ';' is rejected (use a JSON batch)
static ExecOutcome do_exec(XmppMgr *mgr, json_object *params,
                           const std::string &sender, XM_ROLE role,
                           const std::string &reqId);

void XmppMgr::process_json_request(const XmppCmdEntry &entry) {
  XM_ROLE role = role_of_sender(entry.sender);
  json_object *root = json_tokener_parse(entry.cmdMsg.c_str());
  std::string out;
  if (root == NULL) {
    json_object *resp = make_response(
        NULL, NULL, make_error(XMJSON_ERR_PARSE, "Parse error", NULL));
    out = json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN);
    json_object_put(resp);
  } else if (json_object_is_type(root, json_type_array)) {
    int n = json_object_array_length(root);
    if (n == 0) {
      json_object *resp = make_response(
          NULL, NULL,
          make_error(XMJSON_ERR_INVALID_REQUEST, "Invalid Request", NULL));
      out = json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN);
      json_object_put(resp);
    } else {
      json_object *batch = json_object_new_array();
      for (int i = 0; i < n; i++) {
        json_object *resp = serve_one(this, json_object_array_get_idx(root, i),
                                      entry.sender, role);
        if (resp != NULL)
          json_object_array_add(batch, resp);
      }
      if (json_object_array_length(batch) > 0)
        out = json_object_to_json_string_ext(batch, JSON_C_TO_STRING_PLAIN);
      json_object_put(batch);
    }
  } else {
    json_object *resp = serve_one(this, root, entry.sender, role);
    if (resp != NULL) {
      out = json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN);
      json_object_put(resp);
    }
  }
  if (root != NULL)
    json_object_put(root);
  CurrentReq.json = false;
  CurrentReq.reqId = "";
  if (!out.empty()) {
    XMLOG_DBG("json: reply to %s: %s", entry.sender.c_str(), out.c_str());
    XmppProxy.send_reply(out, entry.sender);
  }
}

static json_object *serve_one(XmppMgr *mgr, json_object *req,
                              const std::string &sender, XM_ROLE role) {
  if (!json_object_is_type(req, json_type_object))
    return make_response(
        NULL, NULL,
        make_error(XMJSON_ERR_INVALID_REQUEST, "Invalid Request", NULL));
  json_object *idObj = NULL;
  bool hasId = json_object_object_get_ex(req, "id", &idObj) && idObj != NULL;
  json_object *id = hasId ? json_object_get(idObj) : NULL;
  std::string idkey = id_key(idObj);
  json_object *verObj = NULL;
  if (json_object_object_get_ex(req, "jsonrpc", &verObj) &&
      std::string(json_object_get_string(verObj)) != "2.0")
    return make_response(
        id, NULL,
        make_error(XMJSON_ERR_INVALID_REQUEST, "Invalid Request", NULL));
  json_object *methodObj = NULL;
  if (!json_object_object_get_ex(req, "method", &methodObj) ||
      !json_object_is_type(methodObj, json_type_string))
    return make_response(
        id, NULL,
        make_error(XMJSON_ERR_INVALID_REQUEST, "Invalid Request", NULL));
  std::string method = json_object_get_string(methodObj);
  json_object *params = NULL;
  json_object_object_get_ex(req, "params", &params);

  // duplicate suppression (P3): a repeated id within the window gets the
  // stored response again and the command is not run twice
  if (hasId) {
    std::string cached;
    if (mgr->find_recent_reply(sender, idkey, cached)) {
      XMLOG_INF("json: duplicate request id %s from %s, replaying reply",
                idkey.c_str(), sender.c_str());
      json_object *resp = json_tokener_parse(cached.c_str());
      if (id)
        json_object_put(id);
      return resp;
    }
  }

  json_object *result = NULL;
  json_object *error = NULL;
  if (method == "ping") {
    result = json_object_new_object();
    json_object_object_add(result, "pong", json_object_new_boolean(true));
    json_object_object_add(result, "time",
                           json_object_new_int64((int64_t)time(NULL)));
  } else if (method == "describe") {
    result = mgr->json_describe(role);
  } else if (method == "list_commands") {
    result = mgr->json_list_commands(role);
  } else if (method == "get_manifest") {
    result = mgr->json_get_manifest(role, &error);
  } else if (method == "exec") {
    ExecOutcome o = do_exec(mgr, params, sender, role, idkey);
    result = o.result;
    error = o.error;
  } else {
    error = make_error(XMJSON_ERR_METHOD_NOT_FOUND, "Method not found", NULL);
  }
  if (!hasId) {
    // notification: no reply, whatever happened
    if (result)
      json_object_put(result);
    if (error)
      json_object_put(error);
    return NULL;
  }
  json_object *resp = make_response(id, result, error);
  mgr->remember_reply(
      sender, idkey,
      json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN));
  return resp;
}

json_object *XmppMgr::json_describe(XM_ROLE role) {
  json_object *d = json_object_new_object();
  json_object_object_add(d, "name",
                         json_object_new_string(XmppBotName.c_str()));
  json_object_object_add(d, "version",
                         json_object_new_string(AppVersion.c_str()));
  json_object_object_add(d, "protocol",
                         json_object_new_int(XMJSON_PROTOCOL_VERSION));
  json_object_object_add(d, "jid",
                         json_object_new_string(get_active_jid().c_str()));
  json_object_object_add(d, "primary",
                         json_object_new_string(PrimaryAccount.user.c_str()));
  if (FallbackAccount.configured())
    json_object_object_add(
        d, "fallback", json_object_new_string(FallbackAccount.user.c_str()));
  json_object_object_add(d, "on_fallback",
                         json_object_new_boolean(is_on_fallback()));
  json_object_object_add(d, "role", json_object_new_string(xm_role_name(role)));
  json_object *methods = json_object_new_array();
  const char *names[] = {"ping", "describe", "list_commands", "exec"};
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    json_object_array_add(methods, json_object_new_string(names[i]));
  if (manifest_loaded())
    json_object_array_add(methods, json_object_new_string("get_manifest"));
  json_object_object_add(d, "methods", methods);
  json_object_object_add(d, "manifest",
                         json_object_new_boolean(manifest_loaded()));
  json_object_object_add(d, "notifications", json_object_new_array());
  json_object_array_add(json_object_object_get(d, "notifications"),
                        json_object_new_string("task.done"));
  return d;
}

json_object *XmppMgr::json_list_commands(XM_ROLE role) {
  json_object *arr = json_object_new_array();
  for (int i = 0; i < xmproxy_cmd_table_size; i++) {
    if (!xmproxy_cmd_table[i].cmdsts)
      continue;
    XM_ROLE need = required_role(xmproxy_cmd_table[i].cmd, "");
    json_object *c = json_object_new_object();
    json_object_object_add(
        c, "name", json_object_new_string(xmproxy_cmd_table[i].cmd_name));
    json_object_object_add(
        c, "args", json_object_new_string(xmproxy_cmd_table[i].cmd_arg));
    json_object_object_add(c, "role",
                           json_object_new_string(xm_role_name(need)));
    json_object_object_add(c, "allowed", json_object_new_boolean(role >= need));
    json_object_array_add(arr, c);
  }
  return arr;
}

static ExecOutcome do_exec(XmppMgr *mgr, json_object *params,
                           const std::string &sender, XM_ROLE role,
                           const std::string &reqId) {
  ExecOutcome o = {NULL, NULL};
  json_object *cmdObj = NULL;
  json_object *ctlObj = NULL;
  if (params != NULL && json_object_is_type(params, json_type_object) &&
      json_object_object_get_ex(params, "control", &ctlObj) &&
      json_object_is_type(ctlObj, json_type_string)) {
    // exec {"control": id, "arg": "on"|"off"}: a manifest control by id
    std::string arg;
    json_object *argObj = NULL;
    if (json_object_object_get_ex(params, "arg", &argObj) &&
        json_object_is_type(argObj, json_type_string))
      arg = json_object_get_string(argObj);
    o.result = mgr->json_exec_control(json_object_get_string(ctlObj), arg,
                                      sender, role, reqId, &o.error);
    return o;
  }
  if (params == NULL || !json_object_is_type(params, json_type_object) ||
      !json_object_object_get_ex(params, "cmd", &cmdObj) ||
      !json_object_is_type(cmdObj, json_type_string)) {
    o.error = make_error(XMJSON_ERR_INVALID_PARAMS,
                         "Invalid params: expected {\"cmd\": \"...\"} or "
                         "{\"control\": \"<id>\"}",
                         NULL);
    return o;
  }
  std::string cmd = json_object_get_string(cmdObj);
  if (cmd.find(';') != std::string::npos) {
    o.error = make_error(XMJSON_ERR_INVALID_PARAMS,
                         "Invalid params: one command per exec, use a JSON "
                         "batch for several",
                         NULL);
    return o;
  }
  if (cmd.find_first_not_of(" \t") == std::string::npos) {
    o.error = make_error(XMJSON_ERR_INVALID_PARAMS,
                         "Invalid params: empty command", NULL);
    return o;
  }
  o.result = mgr->json_exec(cmd, sender, role, reqId, &o.error);
  return o;
}

json_object *XmppMgr::json_exec(const std::string &cmd,
                                const std::string &sender, XM_ROLE role,
                                const std::string &reqId, json_object **error) {
  std::deque<std::string> steps = expand_command(cmd);
  // check every step first so a denied alias batch runs nothing at all
  for (std::deque<std::string>::iterator it = steps.begin(); it != steps.end();
       ++it) {
    EXMPP_CMD_TYPES t = ResolveCmdStr(*it);
    XM_ROLE need = required_role(t, *it);
    if (t != EXMPP_CMD_UNKNOWN && role < need) {
      XMLOG_WRN("acl: %s (%s) denied json exec '%s', needs %s", sender.c_str(),
                xm_role_name(role), it->c_str(), xm_role_name(need));
      json_object *data = json_object_new_object();
      json_object_object_add(data, "cmd", json_object_new_string(it->c_str()));
      json_object_object_add(data, "requires",
                             json_object_new_string(xm_role_name(need)));
      *error = make_error(XMJSON_ERR_NOT_AUTHORIZED, "Not authorized", data);
      return NULL;
    }
  }
  CurrentReq.json = true;
  CurrentReq.reqId = reqId;
  json_object *results = json_object_new_array();
  RPC_SRV_RESULT overall = RPC_SRV_RESULT_SUCCESS;
  std::string lastText;
  int lastTask = -1;
  for (std::deque<std::string>::iterator it = steps.begin(); it != steps.end();
       ++it) {
    std::string text;
    RPC_SRV_RESULT res = run_single_command(*it, sender, role, text, NULL);
    json_object *r = json_object_new_object();
    json_object_object_add(r, "cmd", json_object_new_string(it->c_str()));
    json_object_object_add(
        r, "return", json_object_new_string(result_code_name(res).c_str()));
    json_object_object_add(r, "result", json_object_new_string(text.c_str()));
    int task = -1;
    if (res == RPC_SRV_RESULT_IN_PROG && text.compare(0, 7, "taskID=") == 0)
      task = atoi(text.c_str() + 7);
    if (task >= 0)
      json_object_object_add(r, "task", json_object_new_int(task));
    json_object_array_add(results, r);
    if (overall == RPC_SRV_RESULT_SUCCESS && res != RPC_SRV_RESULT_SUCCESS)
      overall = res;
    lastText = text;
    lastTask = task;
  }
  json_object *result = json_object_new_object();
  json_object_object_add(
      result, "return",
      json_object_new_string(result_code_name(overall).c_str()));
  json_object_object_add(result, "result",
                         json_object_new_string(lastText.c_str()));
  if (lastTask >= 0)
    json_object_object_add(result, "task", json_object_new_int(lastTask));
  json_object_object_add(result, "results", results);
  return result;
}

json_object *XmppMgr::json_get_manifest(XM_ROLE role, json_object **error) {
  json_object *m = Manifest.to_json(role);
  if (m == NULL) {
    json_object *data = json_object_new_object();
    std::string reason = Manifest.get_file().empty()
                             ? "no manifest file configured"
                             : Manifest.get_last_error();
    json_object_object_add(data, "reason",
                           json_object_new_string(reason.c_str()));
    *error = make_error(XMJSON_ERR_NO_MANIFEST, "No manifest", data);
    return NULL;
  }
  return m;
}

json_object *XmppMgr::json_exec_control(const std::string &id,
                                        const std::string &arg,
                                        const std::string &sender, XM_ROLE role,
                                        const std::string &reqId,
                                        json_object **error) {
  XmControl control;
  if (!Manifest.find_control(id, control)) {
    json_object *data = json_object_new_object();
    json_object_object_add(data, "control", json_object_new_string(id.c_str()));
    *error = make_error(XMJSON_ERR_INVALID_PARAMS,
                        "Invalid params: unknown control", data);
    return NULL;
  }
  CurrentReq.json = true;
  CurrentReq.reqId = reqId;
  std::vector<StepResult> steps;
  std::string err;
  RPC_SRV_RESULT overall =
      execute_control(control, arg, sender, role, steps, err);
  if (steps.empty()) {
    json_object *data = json_object_new_object();
    json_object_object_add(data, "control", json_object_new_string(id.c_str()));
    if (overall == RPC_SRV_RESULT_ACTION_NOT_ALLOWED) {
      json_object_object_add(
          data, "requires", json_object_new_string(xm_role_name(control.role)));
      *error = make_error(XMJSON_ERR_NOT_AUTHORIZED, "Not authorized", data);
    } else {
      json_object_object_add(data, "reason",
                             json_object_new_string(err.c_str()));
      *error = make_error(XMJSON_ERR_INVALID_PARAMS, "Invalid params", data);
    }
    return NULL;
  }
  json_object *results = json_object_new_array();
  for (size_t i = 0; i < steps.size(); i++) {
    json_object *r = json_object_new_object();
    json_object_object_add(r, "cmd",
                           json_object_new_string(steps[i].cmd.c_str()));
    json_object_object_add(
        r, "return",
        json_object_new_string(result_code_name(steps[i].res).c_str()));
    json_object_object_add(r, "result",
                           json_object_new_string(steps[i].text.c_str()));
    if (steps[i].task >= 0)
      json_object_object_add(r, "task", json_object_new_int(steps[i].task));
    json_object_array_add(results, r);
  }
  json_object *result = json_object_new_object();
  json_object_object_add(result, "control", json_object_new_string(id.c_str()));
  json_object_object_add(
      result, "return",
      json_object_new_string(result_code_name(overall).c_str()));
  json_object_object_add(result, "result",
                         json_object_new_string(steps.back().text.c_str()));
  if (steps.back().task >= 0)
    json_object_object_add(result, "task",
                           json_object_new_int(steps.back().task));
  json_object_object_add(result, "results", results);
  return result;
}
