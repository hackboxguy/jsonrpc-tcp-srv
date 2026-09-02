#include "Acl.h"
#include "XmLog.h"
#include <fstream>
#include <sstream>

const char *xm_role_name(XM_ROLE role) {
  switch (role) {
  case XM_ROLE_ADMIN:
    return "admin";
  case XM_ROLE_OPERATOR:
    return "operator";
  case XM_ROLE_VIEWER:
    return "viewer";
  default:
    return "none";
  }
}
XM_ROLE xm_role_from_name(const std::string &name) {
  if (name == "admin")
    return XM_ROLE_ADMIN;
  if (name == "operator")
    return XM_ROLE_OPERATOR;
  if (name == "viewer")
    return XM_ROLE_VIEWER;
  return XM_ROLE_NONE;
}
XmAcl::XmAcl() : defaultRole(XM_ROLE_VIEWER) {}
int XmAcl::load() {
  std::lock_guard<std::mutex> lock(mtx);
  if (filePath.empty())
    return 0;
  std::ifstream in(filePath.c_str());
  if (!in) {
    XMLOG_WRN("acl: cannot read %s", filePath.c_str());
    return -1;
  }
  std::map<std::string, XM_ROLE> fresh;
  std::string line;
  int lineno = 0;
  while (std::getline(in, line)) {
    lineno++;
    if (line.empty() || line[0] == '#')
      continue;
    std::stringstream ss(line);
    std::string jid, name;
    ss >> jid >> name;
    XM_ROLE role = xm_role_from_name(name);
    if (jid.empty() || role == XM_ROLE_NONE) {
      XMLOG_WRN("acl: %s line %d ignored: '%s'", filePath.c_str(), lineno,
                line.c_str());
      continue;
    }
    fresh[jid] = role;
  }
  roles.swap(fresh);
  XMLOG_INF("acl: loaded %d entr%s from %s, default role %s", (int)roles.size(),
            roles.size() == 1 ? "y" : "ies", filePath.c_str(),
            xm_role_name(defaultRole));
  return (int)roles.size();
}
int XmAcl::save_locked() {
  if (filePath.empty())
    return 0;
  std::ofstream out(filePath.c_str(), std::ios::trunc);
  if (!out) {
    XMLOG_ERR("acl: cannot write %s", filePath.c_str());
    return -1;
  }
  out << "# xmproxy access list: one 'jid role' per line (admin, operator, "
         "viewer)\n";
  for (std::map<std::string, XM_ROLE>::iterator it = roles.begin();
       it != roles.end(); ++it)
    out << it->first << " " << xm_role_name(it->second) << "\n";
  return 0;
}
XM_ROLE XmAcl::role_of(const std::string &jid) {
  std::lock_guard<std::mutex> lock(mtx);
  std::map<std::string, XM_ROLE>::iterator it = roles.find(jid);
  if (it != roles.end())
    return it->second;
  return defaultRole;
}
bool XmAcl::has_entry(const std::string &jid) {
  std::lock_guard<std::mutex> lock(mtx);
  return roles.find(jid) != roles.end();
}
bool XmAcl::set(const std::string &jid, XM_ROLE role) {
  std::lock_guard<std::mutex> lock(mtx);
  roles[jid] = role;
  XMLOG_INF("acl: %s is now %s", jid.c_str(), xm_role_name(role));
  return save_locked() == 0;
}
bool XmAcl::remove(const std::string &jid) {
  std::lock_guard<std::mutex> lock(mtx);
  roles.erase(jid);
  XMLOG_INF("acl: entry for %s removed (default role %s applies)", jid.c_str(),
            xm_role_name(defaultRole));
  return save_locked() == 0;
}
std::vector<std::pair<std::string, XM_ROLE>> XmAcl::entries() {
  std::lock_guard<std::mutex> lock(mtx);
  std::vector<std::pair<std::string, XM_ROLE>> out;
  for (std::map<std::string, XM_ROLE>::iterator it = roles.begin();
       it != roles.end(); ++it)
    out.push_back(*it);
  return out;
}
