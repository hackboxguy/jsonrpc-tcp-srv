// Device-served UI manifest for xmproxysrv (bucket 5). See docs/manifest.md.
//
// A JSON file on the device describes groups of controls. Every control maps
// to existing chat commands or aliases. The daemon validates the file,
// serves a normalized copy to apps (get_manifest) and executes controls by
// id (exec {"control": id}, chat "run <id>"). A control's role is an
// admin-approved grant for that action: it may be lower than the role the
// underlying command would need when typed in chat.
#ifndef __XM_MANIFEST_H_
#define __XM_MANIFEST_H_
#include "Acl.h"
#include <mutex>
#include <string>
#include <time.h>
#include <vector>

struct json_object;

struct XmControl {
  std::string id;
  std::string type; // button | toggle | indicator | text
  std::string label;
  std::string icon;
  std::string action;  // button
  std::string on, off; // toggle
  std::string command; // indicator, text; toggle state
  int interval;        // seconds, indicator/text/toggle state (0 = none)
  std::string unit;    // indicator
  std::string regex;   // indicator/text: first capture group is the value
  std::string match;   // toggle state: substring meaning "on"
  bool confirm;
  XM_ROLE role; // grant: minimum role to use this control
  XmControl() : interval(0), confirm(false), role(XM_ROLE_OPERATOR) {}
};
struct XmGroup {
  std::string id, label, icon;
  std::vector<XmControl> controls;
};
struct XmManifest {
  int version;
  std::string deviceName, deviceDescription, deviceIcon;
  std::vector<XmGroup> groups;
  time_t loaded;
  XmManifest() : version(0), loaded(0) {}
  const XmControl *find(const std::string &id) const;
  int control_count() const;
};

class XmManifestStore {
  std::mutex mtx;
  std::string filePath;
  XmManifest current;
  bool loaded;
  std::string lastError;
  std::vector<std::string> lastWarnings;

public:
  XmManifestStore() : loaded(false) {}
  void set_file(const std::string &p) { filePath = p; }
  std::string get_file() { return filePath; }
  bool is_loaded() { return loaded; }
  std::string get_last_error();
  std::vector<std::string> get_last_warnings();
  // parse and validate a file; on success fills out and returns true
  static bool parse_file(const std::string &path, XmManifest &out,
                         std::string &error,
                         std::vector<std::string> &warnings);
  // parse the configured file and replace the served manifest only on
  // success; a failure keeps the previous manifest and records the error
  bool reload(std::string &error, std::vector<std::string> &warnings);
  bool get(XmManifest &copy);
  bool find_control(const std::string &id, XmControl &out);
  // normalized manifest as JSON for a caller with the given role
  json_object *to_json(XM_ROLE callerRole);
};
#endif
