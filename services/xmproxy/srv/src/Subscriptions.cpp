#include "Subscriptions.h"
#include "XmLog.h"
#include <fstream>
#include <sstream>

int XmSubscriptions::load() {
  std::lock_guard<std::mutex> lock(mtx);
  if (filePath.empty())
    return 0;
  std::ifstream in(filePath.c_str());
  if (!in)
    return 0; // no file yet
  std::map<std::string, std::set<std::string>> fresh;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::stringstream ss(line);
    std::string jid, list;
    ss >> jid >> list;
    if (jid.empty() || list.empty())
      continue;
    std::stringstream ls(list);
    std::string t;
    while (std::getline(ls, t, ','))
      if (!t.empty())
        fresh[jid].insert(t);
  }
  subs.swap(fresh);
  XMLOG_INF("events: loaded subscriptions of %d buddy(ies) from %s",
            (int)subs.size(), filePath.c_str());
  return (int)subs.size();
}
int XmSubscriptions::save_locked() {
  if (filePath.empty())
    return 0;
  std::ofstream out(filePath.c_str(), std::ios::trunc);
  if (!out) {
    XMLOG_ERR("events: cannot write %s", filePath.c_str());
    return -1;
  }
  out << "# xmproxy event subscriptions: 'jid topic,topic' per line\n";
  for (std::map<std::string, std::set<std::string>>::iterator it = subs.begin();
       it != subs.end(); ++it) {
    if (it->second.empty())
      continue;
    out << it->first << " ";
    bool first = true;
    for (std::set<std::string>::iterator t = it->second.begin();
         t != it->second.end(); ++t) {
      out << (first ? "" : ",") << *t;
      first = false;
    }
    out << "\n";
  }
  return 0;
}
std::set<std::string>
XmSubscriptions::add(const std::string &jid,
                     const std::set<std::string> &topics) {
  std::lock_guard<std::mutex> lock(mtx);
  subs[jid].insert(topics.begin(), topics.end());
  save_locked();
  return subs[jid];
}
std::set<std::string>
XmSubscriptions::remove(const std::string &jid,
                        const std::set<std::string> &topics) {
  std::lock_guard<std::mutex> lock(mtx);
  std::map<std::string, std::set<std::string>>::iterator it = subs.find(jid);
  if (it == subs.end())
    return std::set<std::string>();
  if (topics.empty())
    it->second.clear();
  else
    for (std::set<std::string>::const_iterator t = topics.begin();
         t != topics.end(); ++t)
      it->second.erase(*t);
  std::set<std::string> left = it->second;
  if (left.empty())
    subs.erase(it);
  save_locked();
  return left;
}
std::set<std::string> XmSubscriptions::topics_of(const std::string &jid) {
  std::lock_guard<std::mutex> lock(mtx);
  std::map<std::string, std::set<std::string>>::iterator it = subs.find(jid);
  return it == subs.end() ? std::set<std::string>() : it->second;
}
std::vector<std::string>
XmSubscriptions::subscribers_of(const std::string &topic) {
  std::lock_guard<std::mutex> lock(mtx);
  std::vector<std::string> out;
  for (std::map<std::string, std::set<std::string>>::iterator it = subs.begin();
       it != subs.end(); ++it)
    if (it->second.count(topic) || it->second.count("*"))
      out.push_back(it->first);
  return out;
}
std::set<std::string> XmSubscriptions::all_topics() {
  std::lock_guard<std::mutex> lock(mtx);
  std::set<std::string> out;
  for (std::map<std::string, std::set<std::string>>::iterator it = subs.begin();
       it != subs.end(); ++it)
    out.insert(it->second.begin(), it->second.end());
  return out;
}
bool XmSubscriptions::allow_delivery(const std::string &jid) {
  std::lock_guard<std::mutex> lock(mtx);
  time_t minute = time(NULL) / 60;
  Bucket &b = rate[jid];
  if (b.minute != minute) {
    b.minute = minute;
    b.count = 0;
  }
  if (++b.count > XM_NOTIFY_RATE_PER_MIN) {
    if (b.count == XM_NOTIFY_RATE_PER_MIN + 1)
      XMLOG_WRN(
          "events: rate limit reached for %s, dropping events this minute",
          jid.c_str());
    return false;
  }
  return true;
}
int XmSubscriptions::subscriber_count() {
  std::lock_guard<std::mutex> lock(mtx);
  return (int)subs.size();
}
