// Event subscriptions for xmproxysrv (bucket 6). See docs/protocol.md.
//
// A buddy subscribes to topics: a pollable manifest control id, "system",
// "task", "heartbeat" or "*". Subscriptions are persisted per bare JID in a
// text file ("jid topic1,topic2" per line) and survive restarts.
#ifndef __XM_SUBSCRIPTIONS_H_
#define __XM_SUBSCRIPTIONS_H_
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <time.h>
#include <vector>

#define XM_NOTIFY_RATE_PER_MIN                                                 \
  60 // per subscriber, beyond that events are dropped

class XmSubscriptions {
  std::mutex mtx;
  std::string filePath;
  std::map<std::string, std::set<std::string>> subs; // jid -> topics
  struct Bucket {
    time_t minute;
    int count;
    Bucket() : minute(0), count(0) {}
  };
  std::map<std::string, Bucket> rate; // jid -> notifications this minute
  int save_locked();

public:
  void set_file(const std::string &p) { filePath = p; }
  std::string get_file() { return filePath; }
  int load();
  // add topics for jid; returns the resulting topic set
  std::set<std::string> add(const std::string &jid,
                            const std::set<std::string> &topics);
  // remove topics (empty set = all); returns the resulting topic set
  std::set<std::string> remove(const std::string &jid,
                               const std::set<std::string> &topics);
  std::set<std::string> topics_of(const std::string &jid);
  // jids subscribed to topic or to "*"
  std::vector<std::string> subscribers_of(const std::string &topic);
  // every topic anybody subscribed to (used by the poller)
  std::set<std::string> all_topics();
  // rate limit check for one delivery to jid
  bool allow_delivery(const std::string &jid);
  int subscriber_count();
};
#endif
