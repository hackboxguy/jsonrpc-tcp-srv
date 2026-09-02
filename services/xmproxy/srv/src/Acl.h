// Role based access control for xmproxysrv (bucket 3).
//
// Roles: admin > operator > viewer. The admin buddies from the login file
// are always admin. Other roster members get the role listed in the ACL file
// or, when absent, the default role (viewer). JIDs outside the roster are
// never answered at all (handled before this class is consulted).
//
// ACL file format: one "jid role" per line, '#' starts a comment.
#ifndef __XM_ACL_H_
#define __XM_ACL_H_
#include <map>
#include <mutex>
#include <string>
#include <vector>

typedef enum XM_ROLE_T {
  XM_ROLE_NONE = 0, // not authorized at all
  XM_ROLE_VIEWER,
  XM_ROLE_OPERATOR,
  XM_ROLE_ADMIN
} XM_ROLE;

const char *xm_role_name(XM_ROLE role);
// returns XM_ROLE_NONE for an unknown name
XM_ROLE xm_role_from_name(const std::string &name);

class XmAcl {
  std::mutex mtx;
  std::string filePath;
  std::map<std::string, XM_ROLE> roles; // bare jid -> role
  XM_ROLE defaultRole;
  int save_locked();

public:
  XmAcl();
  void set_file(const std::string &path) { filePath = path; }
  std::string get_file() { return filePath; }
  void set_default_role(XM_ROLE role) { defaultRole = role; }
  XM_ROLE get_default_role() { return defaultRole; }
  // (re)load from the file; returns number of entries, -1 if the file is
  // configured but cannot be read. No file configured: keeps the in-memory
  // list and returns 0.
  int load();
  // role of a roster member (never for JIDs outside the roster)
  XM_ROLE role_of(const std::string &jid);
  bool has_entry(const std::string &jid);
  // set/remove an entry and persist when a file is configured;
  // returns false when persisting failed
  bool set(const std::string &jid, XM_ROLE role);
  bool remove(const std::string &jid);
  std::vector<std::pair<std::string, XM_ROLE>> entries();
};
#endif
