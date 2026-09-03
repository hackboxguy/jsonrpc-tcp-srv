// Subscriptions, indicator polling and event notifications (bucket 6).
// See docs/protocol.md ("Events") and docs/manifest.md ("Indicators").
#include "XmppMgr.h"
#include <json-c/json.h>
#include <regex.h>
#include <sstream>

#define XMJSON_ERR_INVALID_PARAMS -32602
#define XMJSON_ERR_NOT_AUTHORIZED -32001

namespace {
std::string trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos)
    return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}
json_object *error_obj(int code, const char *msg, json_object *data) {
  json_object *e = json_object_new_object();
  json_object_object_add(e, "code", json_object_new_int(code));
  json_object_object_add(e, "message", json_object_new_string(msg));
  if (data)
    json_object_object_add(e, "data", data);
  return e;
}
json_object *topics_array(const std::set<std::string> &topics) {
  json_object *arr = json_object_new_array();
  for (std::set<std::string>::const_iterator t = topics.begin();
       t != topics.end(); ++t)
    json_object_array_add(arr, json_object_new_string(t->c_str()));
  return arr;
}
bool pollable(const XmControl &c) {
  return !c.command.empty() && c.interval > 0;
}
} // namespace

/* ---- publishing -------------------------------------------------------- */
// params may be NULL (heartbeat builds its own); takes ownership of params
void XmppMgr::publish(const std::string &topic, json_object *params,
                      const std::string &exceptJid) {
  std::vector<std::string> jids = Subs.subscribers_of(topic);
  if (jids.empty()) {
    if (params)
      json_object_put(params);
    return;
  }
  for (size_t i = 0; i < jids.size(); i++) {
    if (jids[i] == exceptJid)
      continue;
    json_object *copy = json_object_new_object();
    if (params) {
      json_object_object_foreach(params, key, val) {
        json_object_object_add(copy, key, json_object_get(val));
      }
    }
    publish_to(jids[i], topic, copy);
  }
  if (params)
    json_object_put(params);
}
// takes ownership of params; adds topic and time, applies the rate limit
void XmppMgr::publish_to(const std::string &jid, const std::string &topic,
                         json_object *params) {
  if (params == NULL)
    params = json_object_new_object();
  if (!Subs.allow_delivery(jid)) {
    json_object_put(params);
    return;
  }
  json_object_object_add(params, "topic",
                         json_object_new_string(topic.c_str()));
  json_object_object_add(params, "time",
                         json_object_new_int64((int64_t)time(NULL)));
  if (topic == "heartbeat") {
    json_object_object_add(
        params, "uptime",
        json_object_new_int64((int64_t)(time(NULL) - StartedAt)));
    json_object_object_add(params, "jid",
                           json_object_new_string(get_active_jid().c_str()));
    json_object_object_add(params, "on_fallback",
                           json_object_new_boolean(is_on_fallback()));
  }
  json_object *note = json_object_new_object();
  json_object_object_add(note, "jsonrpc", json_object_new_string("2.0"));
  json_object_object_add(note, "method", json_object_new_string("event"));
  json_object_object_add(note, "params", params);
  const char *text =
      json_object_to_json_string_ext(note, JSON_C_TO_STRING_PLAIN);
  XMLOG_DBG("events: %s -> %s: %s", topic.c_str(), jid.c_str(), text);
  XmppProxy.send_reply(text, jid);
  json_object_put(note);
}
void XmppMgr::set_pending_system_event(const std::string &ev) {
  std::lock_guard<std::mutex> lock(systemMutex);
  pendingSystemEvents.push_back(ev);
}
void XmppMgr::emit_system_events() {
  std::deque<std::string> events;
  {
    std::lock_guard<std::mutex> lock(systemMutex);
    events.swap(pendingSystemEvents);
  }
  for (size_t i = 0; i < events.size(); i++) {
    json_object *p = json_object_new_object();
    json_object_object_add(p, "event",
                           json_object_new_string(events[i].c_str()));
    json_object_object_add(p, "jid",
                           json_object_new_string(get_active_jid().c_str()));
    json_object_object_add(p, "on_fallback",
                           json_object_new_boolean(is_on_fallback()));
    publish("system", p);
  }
}

/* ---- polling ----------------------------------------------------------- */
int XmppMgr::poll_loop() {
  PollerRunning = true;
  while (!PollerStop && !XmppProxy.getForcedDisconnect()) {
    usleep(1000000);
    if (!XmppProxy.get_connected_status())
      continue;
    std::set<std::string> topics = Subs.all_topics();
    if (topics.empty())
      continue;
    bool all = topics.count("*") > 0;
    XmManifest m;
    if (!Manifest.get(m))
      continue;
    time_t now = time(NULL);
    for (size_t g = 0; g < m.groups.size(); g++) {
      for (size_t c = 0; c < m.groups[g].controls.size(); c++) {
        const XmControl &ctl = m.groups[g].controls[c];
        if (!pollable(ctl) || (!all && !topics.count(ctl.id)))
          continue;
        bool due = false;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          ControlState &st = controlStates[ctl.id];
          if (!st.inflight && now - st.lastPoll >= ctl.interval) {
            st.inflight = true;
            st.lastPoll = now;
            due = true;
          }
        }
        if (due)
          enqueue_poll(ctl.id, "");
      }
    }
  }
  PollerRunning = false;
  return 0;
}
void XmppMgr::enqueue_poll(const std::string &id,
                           const std::string &initialTo) {
  XmppCmdEntry e("", "");
  e.pollControl = id;
  e.pollInitialTo = initialTo;
  {
    std::lock_guard<std::mutex> lock(cmdMutex);
    if (processCmd.size() >= XMPP_MAX_PENDING_CMDS) {
      XMLOG_WRN("events: queue full, poll of '%s' skipped", id.c_str());
      std::lock_guard<std::mutex> slock(stateMutex);
      controlStates[id].inflight = false;
      return;
    }
    processCmd.push_back(e);
  }
  XmppCmdProcessThread.wakeup_thread();
}
// worker thread: run the control's read command; synchronous results are
// finished here, asynchronous ones (shellcmd) when their completion arrives
void XmppMgr::poll_control(const std::string &id,
                           const std::string &initialTo) {
  XmControl ctl;
  if (!Manifest.find_control(id, ctl) || ctl.command.empty()) {
    std::lock_guard<std::mutex> lock(stateMutex);
    controlStates[id].inflight = false;
    return;
  }
  std::deque<std::string> steps = expand_command(ctl.command);
  CurrentReq.clear();
  CurrentReq.pollControl = id;
  CurrentReq.pollInitialTo = initialTo;
  CurrentReq.shellOutput =
      !steps.empty() && ResolveCmdStr(steps.back()) == EXMPP_CMD_SHELLCMD;
  std::string text;
  RPC_SRV_RESULT res = RPC_SRV_RESULT_FAIL;
  for (std::deque<std::string>::iterator it = steps.begin(); it != steps.end();
       ++it) {
    text.clear();
    res = run_single_command(*it, "poller", XM_ROLE_ADMIN, text, NULL);
  }
  CurrentReq.clear();
  if (res == RPC_SRV_RESULT_IN_PROG)
    return; // completion event finishes the poll
  finish_poll(id, initialTo, res, text);
}
// regex: first capture group; toggle: match substring -> on/off; else trimmed
bool XmppMgr::extract_value(const XmControl &c, const std::string &raw,
                            std::string &value) {
  if (c.type == "toggle") {
    if (c.match.empty())
      value = trim(raw);
    else
      value = raw.find(c.match) != std::string::npos ? "on" : "off";
    return true;
  }
  if (!c.regex.empty()) {
    regex_t re;
    if (regcomp(&re, c.regex.c_str(), REG_EXTENDED) != 0) {
      value = "";
      return false;
    }
    regmatch_t m[2];
    bool ok = regexec(&re, raw.c_str(), 2, m, 0) == 0;
    if (ok) {
      int idx = (m[1].rm_so >= 0) ? 1 : 0;
      value = raw.substr(m[idx].rm_so, m[idx].rm_eo - m[idx].rm_so);
    } else
      value = "";
    regfree(&re);
    return ok;
  }
  value = trim(raw);
  return true;
}
void XmppMgr::finish_poll(const std::string &id, const std::string &initialTo,
                          RPC_SRV_RESULT res, const std::string &raw) {
  XmControl ctl;
  bool known = Manifest.find_control(id, ctl);
  std::string value, error;
  if (!known)
    error = "control removed";
  else if (res != RPC_SRV_RESULT_SUCCESS)
    error = result_code_name(res);
  else if (!extract_value(ctl, raw, value))
    error = "no match";
  bool changed;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    ControlState &st = controlStates[id];
    changed = !st.valid || st.value != value || st.error != error;
    st.valid = true;
    st.value = value;
    st.error = error;
    st.inflight = false;
    st.lastPoll = time(NULL);
  }
  if (!known)
    return;
  json_object *p = json_object_new_object();
  json_object_object_add(p, "control", json_object_new_string(id.c_str()));
  json_object_object_add(p, "type", json_object_new_string(ctl.type.c_str()));
  if (error.empty()) {
    json_object_object_add(p, "value", json_object_new_string(value.c_str()));
    if (!ctl.unit.empty())
      json_object_object_add(p, "unit",
                             json_object_new_string(ctl.unit.c_str()));
  } else
    json_object_object_add(p, "error", json_object_new_string(error.c_str()));
  if (changed)
    XMLOG_INF("events: '%s' = '%s'%s%s", id.c_str(), value.c_str(),
              error.empty() ? "" : " error ", error.c_str());
  if (!initialTo.empty()) {
    json_object *copy = json_object_new_object();
    json_object_object_foreach(p, key, val) {
      json_object_object_add(copy, key, json_object_get(val));
    }
    publish_to(initialTo, id, copy);
  }
  if (changed)
    publish(id, p, initialTo);
  else
    json_object_put(p);
}

/* ---- subscriptions ----------------------------------------------------- */
bool XmppMgr::topic_valid(const std::string &topic, XM_ROLE role,
                          std::string &why, int *errCode) {
  *errCode = XMJSON_ERR_INVALID_PARAMS;
  if (topic == "system" || topic == "task" || topic == "heartbeat" ||
      topic == "*")
    return true;
  XmControl c;
  if (!Manifest.find_control(topic, c)) {
    why = "unknown topic";
    return false;
  }
  if (!pollable(c)) {
    why = "control has no command and interval";
    return false;
  }
  if (role < c.role) {
    why = std::string("requires ") + xm_role_name(c.role);
    *errCode = XMJSON_ERR_NOT_AUTHORIZED;
    return false;
  }
  return true;
}
// initial values: poll every subscribed control right away for this jid
void XmppMgr::request_initial_polls(const std::string &jid,
                                    const std::set<std::string> &topics) {
  XmManifest m;
  if (!Manifest.get(m))
    return;
  bool all = topics.count("*") > 0;
  for (size_t g = 0; g < m.groups.size(); g++)
    for (size_t c = 0; c < m.groups[g].controls.size(); c++) {
      const XmControl &ctl = m.groups[g].controls[c];
      if (pollable(ctl) && (all || topics.count(ctl.id))) {
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          controlStates[ctl.id].inflight = true;
          controlStates[ctl.id].lastPoll = time(NULL);
        }
        enqueue_poll(ctl.id, jid);
      }
    }
}
json_object *XmppMgr::json_subscribe(json_object *params,
                                     const std::string &sender, XM_ROLE role,
                                     json_object **error) {
  json_object *arr = NULL;
  if (params == NULL || !json_object_is_type(params, json_type_object) ||
      !json_object_object_get_ex(params, "topics", &arr) ||
      !json_object_is_type(arr, json_type_array) ||
      json_object_array_length(arr) == 0) {
    *error = error_obj(XMJSON_ERR_INVALID_PARAMS,
                       "Invalid params: expected {\"topics\": [...]}", NULL);
    return NULL;
  }
  std::set<std::string> topics;
  int n = json_object_array_length(arr);
  for (int i = 0; i < n; i++) {
    json_object *t = json_object_array_get_idx(arr, i);
    if (!json_object_is_type(t, json_type_string)) {
      *error = error_obj(XMJSON_ERR_INVALID_PARAMS,
                         "Invalid params: topics must be strings", NULL);
      return NULL;
    }
    std::string topic = json_object_get_string(t);
    std::string why;
    int code;
    if (!topic_valid(topic, role, why, &code)) {
      json_object *data = json_object_new_object();
      json_object_object_add(data, "topic",
                             json_object_new_string(topic.c_str()));
      json_object_object_add(data, "reason",
                             json_object_new_string(why.c_str()));
      *error = error_obj(code,
                         code == XMJSON_ERR_NOT_AUTHORIZED ? "Not authorized"
                                                           : "Invalid params",
                         data);
      return NULL;
    }
    topics.insert(topic);
  }
  std::set<std::string> now = Subs.add(sender, topics);
  XMLOG_INF("events: %s subscribed to %d topic(s)", sender.c_str(),
            (int)topics.size());
  request_initial_polls(sender, topics);
  json_object *result = json_object_new_object();
  json_object_object_add(result, "topics", topics_array(now));
  return result;
}
json_object *XmppMgr::json_unsubscribe(json_object *params,
                                       const std::string &sender,
                                       json_object **error) {
  std::set<std::string> topics; // empty: everything
  json_object *arr = NULL;
  if (params != NULL && json_object_is_type(params, json_type_object) &&
      json_object_object_get_ex(params, "topics", &arr)) {
    if (!json_object_is_type(arr, json_type_array)) {
      *error = error_obj(XMJSON_ERR_INVALID_PARAMS,
                         "Invalid params: topics must be an array", NULL);
      return NULL;
    }
    int n = json_object_array_length(arr);
    for (int i = 0; i < n; i++) {
      json_object *t = json_object_array_get_idx(arr, i);
      if (json_object_is_type(t, json_type_string))
        topics.insert(json_object_get_string(t));
    }
  }
  std::set<std::string> now = Subs.remove(sender, topics);
  json_object *result = json_object_new_object();
  json_object_object_add(result, "topics", topics_array(now));
  return result;
}
json_object *XmppMgr::json_get_subscriptions(const std::string &sender) {
  json_object *result = json_object_new_object();
  json_object_object_add(result, "topics",
                         topics_array(Subs.topics_of(sender)));
  return result;
}
// chat: watch | watch <topic,...> ; unwatch [<topic,...>]
RPC_SRV_RESULT XmppMgr::proc_cmd_watch(std::string msg, std::string &returnval,
                                       const std::string &sender,
                                       XM_ROLE role) {
  stringstream ss(msg);
  std::string cmd, list;
  ss >> cmd >> list;
  if (list.empty()) {
    std::set<std::string> t = Subs.topics_of(sender);
    returnval = "\n";
    for (std::set<std::string>::iterator it = t.begin(); it != t.end(); ++it)
      returnval += *it + "\n";
    return RPC_SRV_RESULT_SUCCESS;
  }
  std::set<std::string> topics;
  stringstream ls(list);
  std::string topic;
  while (std::getline(ls, topic, ',')) {
    if (topic.empty())
      continue;
    std::string why;
    int code;
    if (!topic_valid(topic, role, why, &code)) {
      returnval = topic + ": " + why;
      return code == XMJSON_ERR_NOT_AUTHORIZED
                 ? RPC_SRV_RESULT_ACTION_NOT_ALLOWED
                 : RPC_SRV_RESULT_ARG_ERROR;
    }
    topics.insert(topic);
  }
  Subs.add(sender, topics);
  request_initial_polls(sender, topics);
  returnval = "watching " + list;
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT XmppMgr::proc_cmd_unwatch(std::string msg,
                                         std::string &returnval,
                                         const std::string &sender) {
  stringstream ss(msg);
  std::string cmd, list;
  ss >> cmd >> list;
  std::set<std::string> topics;
  stringstream ls(list);
  std::string topic;
  while (std::getline(ls, topic, ','))
    if (!topic.empty())
      topics.insert(topic);
  std::set<std::string> left = Subs.remove(sender, topics);
  returnval = left.empty() ? "no subscriptions" : "";
  for (std::set<std::string>::iterator it = left.begin(); it != left.end();
       ++it)
    returnval += (it == left.begin() ? "" : ",") + *it;
  return RPC_SRV_RESULT_SUCCESS;
}
