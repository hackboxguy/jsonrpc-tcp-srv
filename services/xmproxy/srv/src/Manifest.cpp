#include "Manifest.h"
#include "XmLog.h"
#include <fstream>
#include <json-c/json.h>
#include <set>
#include <sstream>

const XmControl *XmManifest::find(const std::string &id) const {
  for (size_t g = 0; g < groups.size(); g++)
    for (size_t c = 0; c < groups[g].controls.size(); c++)
      if (groups[g].controls[c].id == id)
        return &groups[g].controls[c];
  return NULL;
}
int XmManifest::control_count() const {
  int n = 0;
  for (size_t g = 0; g < groups.size(); g++)
    n += (int)groups[g].controls.size();
  return n;
}

namespace {
bool get_str(json_object *o, const char *key, std::string &out) {
  json_object *v = NULL;
  if (!json_object_object_get_ex(o, key, &v) || v == NULL)
    return false;
  if (!json_object_is_type(v, json_type_string))
    return false;
  out = json_object_get_string(v);
  return true;
}
bool has_key(json_object *o, const char *key) {
  json_object *v = NULL;
  return json_object_object_get_ex(o, key, &v);
}
bool valid_id(const std::string &s) {
  if (s.empty() || s.size() > 64)
    return false;
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return false;
  }
  return true;
}
std::string where(const XmGroup &g, const XmControl &c) {
  return "group '" + g.id + "' control '" + c.id + "'";
}
bool parse_command_field(json_object *o, const char *key, std::string &out,
                         const std::string &ctx, std::string &error) {
  if (!get_str(o, key, out))
    return true; // absent is handled by the type rules
  if (out.find(';') != std::string::npos) {
    error = ctx + ": '" + key +
            "' must be one command (define an alias for a batch)";
    return false;
  }
  if (out.find_first_not_of(" \t") == std::string::npos) {
    error = ctx + ": '" + key + "' is empty";
    return false;
  }
  return true;
}
} // namespace

bool XmManifestStore::parse_file(const std::string &path, XmManifest &out,
                                 std::string &error,
                                 std::vector<std::string> &warnings) {
  std::ifstream in(path.c_str());
  if (!in) {
    error = "cannot read " + path;
    return false;
  }
  std::stringstream buf;
  buf << in.rdbuf();
  json_tokener *tok = json_tokener_new();
  json_object *root =
      json_tokener_parse_ex(tok, buf.str().c_str(), (int)buf.str().size());
  if (root == NULL) {
    error = std::string("invalid JSON: ") +
            json_tokener_error_desc(json_tokener_get_error(tok));
    json_tokener_free(tok);
    return false;
  }
  json_tokener_free(tok);
  XmManifest m;
  bool ok = true;
  do {
    if (!json_object_is_type(root, json_type_object)) {
      error = "top level must be an object";
      ok = false;
      break;
    }
    json_object *v = NULL;
    if (!json_object_object_get_ex(root, "manifest", &v) ||
        !json_object_is_type(v, json_type_int) || json_object_get_int(v) != 1) {
      error = "'manifest' must be 1";
      ok = false;
      break;
    }
    m.version = 1;
    json_object *dev = NULL;
    if (!json_object_object_get_ex(root, "device", &dev) ||
        !json_object_is_type(dev, json_type_object) ||
        !get_str(dev, "name", m.deviceName) || m.deviceName.empty()) {
      error = "'device.name' is required";
      ok = false;
      break;
    }
    get_str(dev, "description", m.deviceDescription);
    get_str(dev, "icon", m.deviceIcon);
    json_object *groups = NULL;
    if (!json_object_object_get_ex(root, "groups", &groups) ||
        !json_object_is_type(groups, json_type_array)) {
      error = "'groups' must be an array";
      ok = false;
      break;
    }
    std::set<std::string> ids;
    int ng = json_object_array_length(groups);
    for (int gi = 0; gi < ng && ok; gi++) {
      json_object *go = json_object_array_get_idx(groups, gi);
      XmGroup g;
      if (!json_object_is_type(go, json_type_object) ||
          !get_str(go, "id", g.id) || !valid_id(g.id)) {
        std::stringstream e;
        e << "group " << gi
          << ": 'id' missing or invalid (letters, digits, _ -)";
        error = e.str();
        ok = false;
        break;
      }
      if (!ids.insert("g:" + g.id).second) {
        error = "duplicate group id '" + g.id + "'";
        ok = false;
        break;
      }
      if (!get_str(go, "label", g.label))
        g.label = g.id;
      get_str(go, "icon", g.icon);
      json_object *controls = NULL;
      if (!json_object_object_get_ex(go, "controls", &controls) ||
          !json_object_is_type(controls, json_type_array)) {
        error = "group '" + g.id + "': 'controls' must be an array";
        ok = false;
        break;
      }
      int nc = json_object_array_length(controls);
      for (int ci = 0; ci < nc && ok; ci++) {
        json_object *co = json_object_array_get_idx(controls, ci);
        XmControl c;
        if (!json_object_is_type(co, json_type_object) ||
            !get_str(co, "id", c.id) || !valid_id(c.id)) {
          std::stringstream e;
          e << "group '" << g.id << "' control " << ci
            << ": 'id' missing or invalid";
          error = e.str();
          ok = false;
          break;
        }
        std::string ctx = where(g, c);
        if (!ids.insert("c:" + c.id).second) {
          error = ctx + ": duplicate control id";
          ok = false;
          break;
        }
        if (!get_str(co, "type", c.type) ||
            (c.type != "button" && c.type != "toggle" &&
             c.type != "indicator" && c.type != "text")) {
          error = ctx + ": 'type' must be button, toggle, indicator or text";
          ok = false;
          break;
        }
        if (!get_str(co, "label", c.label))
          c.label = c.id;
        get_str(co, "icon", c.icon);
        get_str(co, "unit", c.unit);
        get_str(co, "regex", c.regex);
        get_str(co, "match", c.match);
        if (!parse_command_field(co, "action", c.action, ctx, error) ||
            !parse_command_field(co, "on", c.on, ctx, error) ||
            !parse_command_field(co, "off", c.off, ctx, error) ||
            !parse_command_field(co, "command", c.command, ctx, error)) {
          ok = false;
          break;
        }
        json_object *iv = NULL;
        if (json_object_object_get_ex(co, "interval", &iv)) {
          if (!json_object_is_type(iv, json_type_int) ||
              json_object_get_int(iv) < 5) {
            error =
                ctx + ": 'interval' must be an integer of at least 5 seconds";
            ok = false;
            break;
          }
          c.interval = json_object_get_int(iv);
        }
        json_object *cf = NULL;
        if (json_object_object_get_ex(co, "confirm", &cf)) {
          if (!json_object_is_type(cf, json_type_boolean)) {
            error = ctx + ": 'confirm' must be true or false";
            ok = false;
            break;
          }
          c.confirm = json_object_get_boolean(cf);
        }
        std::string rolename;
        if (get_str(co, "role", rolename)) {
          c.role = xm_role_from_name(rolename);
          if (c.role == XM_ROLE_NONE) {
            error = ctx + ": 'role' must be admin, operator or viewer";
            ok = false;
            break;
          }
        } else if (has_key(co, "role")) {
          error = ctx + ": 'role' must be a string";
          ok = false;
          break;
        } else {
          c.role = (c.type == "indicator" || c.type == "text")
                       ? XM_ROLE_VIEWER
                       : XM_ROLE_OPERATOR;
        }
        if (c.type == "button" && c.action.empty()) {
          error = ctx + ": button needs 'action'";
          ok = false;
          break;
        }
        if (c.type == "toggle" && (c.on.empty() || c.off.empty())) {
          error = ctx + ": toggle needs 'on' and 'off'";
          ok = false;
          break;
        }
        if ((c.type == "indicator" || c.type == "text") && c.command.empty()) {
          error = ctx + ": " + c.type + " needs 'command'";
          ok = false;
          break;
        }
        if (c.type == "toggle" && !c.command.empty() && c.match.empty())
          warnings.push_back(ctx + ": toggle state 'command' without 'match', "
                                   "state will be shown as text");
        g.controls.push_back(c);
      }
      if (ok)
        m.groups.push_back(g);
    }
  } while (0);
  json_object_put(root);
  if (!ok)
    return false;
  if (m.groups.empty())
    warnings.push_back("manifest has no groups");
  m.loaded = time(NULL);
  out = m;
  return true;
}

bool XmManifestStore::reload(std::string &error,
                             std::vector<std::string> &warnings) {
  if (filePath.empty()) {
    error = "no manifest file configured (--manifest)";
    std::lock_guard<std::mutex> lock(mtx);
    lastError = error;
    return false;
  }
  XmManifest fresh;
  bool ok = parse_file(filePath, fresh, error, warnings);
  std::lock_guard<std::mutex> lock(mtx);
  lastWarnings = warnings;
  if (!ok) {
    lastError = error;
    XMLOG_ERR("manifest: %s (%s kept)", error.c_str(),
              loaded ? "previous manifest" : "nothing");
    return false;
  }
  current = fresh;
  loaded = true;
  lastError = "";
  XMLOG_INF("manifest: loaded '%s' with %d group(s), %d control(s) from %s",
            fresh.deviceName.c_str(), (int)fresh.groups.size(),
            fresh.control_count(), filePath.c_str());
  for (size_t i = 0; i < warnings.size(); i++)
    XMLOG_WRN("manifest: %s", warnings[i].c_str());
  return true;
}
std::string XmManifestStore::get_last_error() {
  std::lock_guard<std::mutex> lock(mtx);
  return lastError;
}
std::vector<std::string> XmManifestStore::get_last_warnings() {
  std::lock_guard<std::mutex> lock(mtx);
  return lastWarnings;
}
bool XmManifestStore::get(XmManifest &copy) {
  std::lock_guard<std::mutex> lock(mtx);
  if (!loaded)
    return false;
  copy = current;
  return true;
}
bool XmManifestStore::find_control(const std::string &id, XmControl &out) {
  std::lock_guard<std::mutex> lock(mtx);
  if (!loaded)
    return false;
  const XmControl *c = current.find(id);
  if (c == NULL)
    return false;
  out = *c;
  return true;
}
json_object *XmManifestStore::to_json(XM_ROLE callerRole) {
  XmManifest m;
  if (!get(m))
    return NULL;
  json_object *root = json_object_new_object();
  json_object_object_add(root, "manifest", json_object_new_int(1));
  json_object *dev = json_object_new_object();
  json_object_object_add(dev, "name",
                         json_object_new_string(m.deviceName.c_str()));
  if (!m.deviceDescription.empty())
    json_object_object_add(dev, "description",
                           json_object_new_string(m.deviceDescription.c_str()));
  if (!m.deviceIcon.empty())
    json_object_object_add(dev, "icon",
                           json_object_new_string(m.deviceIcon.c_str()));
  json_object_object_add(root, "device", dev);
  json_object_object_add(root, "loaded",
                         json_object_new_int64((int64_t)m.loaded));
  json_object *groups = json_object_new_array();
  for (size_t g = 0; g < m.groups.size(); g++) {
    const XmGroup &G = m.groups[g];
    json_object *go = json_object_new_object();
    json_object_object_add(go, "id", json_object_new_string(G.id.c_str()));
    json_object_object_add(go, "label",
                           json_object_new_string(G.label.c_str()));
    if (!G.icon.empty())
      json_object_object_add(go, "icon",
                             json_object_new_string(G.icon.c_str()));
    json_object *controls = json_object_new_array();
    for (size_t ci = 0; ci < G.controls.size(); ci++) {
      const XmControl &C = G.controls[ci];
      json_object *co = json_object_new_object();
      json_object_object_add(co, "id", json_object_new_string(C.id.c_str()));
      json_object_object_add(co, "type",
                             json_object_new_string(C.type.c_str()));
      json_object_object_add(co, "label",
                             json_object_new_string(C.label.c_str()));
      if (!C.icon.empty())
        json_object_object_add(co, "icon",
                               json_object_new_string(C.icon.c_str()));
      if (!C.action.empty())
        json_object_object_add(co, "action",
                               json_object_new_string(C.action.c_str()));
      if (!C.on.empty())
        json_object_object_add(co, "on", json_object_new_string(C.on.c_str()));
      if (!C.off.empty())
        json_object_object_add(co, "off",
                               json_object_new_string(C.off.c_str()));
      if (!C.command.empty())
        json_object_object_add(co, "command",
                               json_object_new_string(C.command.c_str()));
      if (C.interval > 0)
        json_object_object_add(co, "interval", json_object_new_int(C.interval));
      if (!C.unit.empty())
        json_object_object_add(co, "unit",
                               json_object_new_string(C.unit.c_str()));
      if (!C.regex.empty())
        json_object_object_add(co, "regex",
                               json_object_new_string(C.regex.c_str()));
      if (!C.match.empty())
        json_object_object_add(co, "match",
                               json_object_new_string(C.match.c_str()));
      json_object_object_add(co, "confirm", json_object_new_boolean(C.confirm));
      json_object_object_add(co, "role",
                             json_object_new_string(xm_role_name(C.role)));
      json_object_object_add(co, "allowed",
                             json_object_new_boolean(callerRole >= C.role));
      json_object_array_add(controls, co);
    }
    json_object_object_add(go, "controls", controls);
    json_object_array_add(groups, go);
  }
  json_object_object_add(root, "groups", groups);
  return root;
}
