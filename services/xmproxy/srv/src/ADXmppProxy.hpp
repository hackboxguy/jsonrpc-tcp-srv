// gloox based XMPP client wrapper for xmproxysrv.
//
// Threading model (bucket 1 of the m2m-extension work):
//  - connect() runs on the XMPP client thread and owns the gloox Client for
//    the whole session. In TCP mode it polls recv() with a short timeout and
//    drains the outbound queue between polls, so every gloox call happens on
//    that one thread. In BOSH mode recv() blocks (see CLAUDE.md, timed recv
//    causes "too many requests"), so queued items are sent from the calling
//    thread under clientMutex: a documented limitation of BOSH mode.
//  - Other threads never touch the gloox Client directly. They enqueue
//    messages, pings and roster operations, or read the roster mirror.
#ifndef __ADXMPP_PROXY_H_
#define __ADXMPP_PROXY_H_

#define CLIENT_TEST
#define CLIENTBASE_TEST

#include <atomic>
#include <deque>
#include <gloox/chatstatefilter.h>
#include <gloox/chatstatehandler.h>
#include <gloox/client.h>
#include <gloox/connectionbosh.h>
#include <gloox/connectionhttpproxy.h>
#include <gloox/connectionlistener.h>
#include <gloox/connectionsocks5proxy.h>
#include <gloox/connectiontcpclient.h>
#include <gloox/connectiontls.h>
#include <gloox/disco.h>
#include <gloox/eventhandler.h>
#include <gloox/gloox.h>
#include <gloox/lastactivity.h>
#include <gloox/loghandler.h>
#include <gloox/logsink.h>
#include <gloox/message.h>
#include <gloox/messageeventfilter.h>
#include <gloox/messageeventhandler.h>
#include <gloox/messagehandler.h>
#include <gloox/messagesessionhandler.h>
#include <gloox/rostermanager.h>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>
using namespace std;
using namespace gloox;
/* ------------------------------------------------------------------------- */
class ADXmppProducer; // subject
class ADXmppConsumer  // observer
{
public:
  virtual int onXmppMessage(std::string msg, std::string sender,
                            ADXmppProducer *pObj) = 0;
  virtual ~ADXmppConsumer() {};
};
class ADXmppProducer {
  static int IDGenerator;
  ADXmppConsumer *pConsumer; // consumer object where notification goes to
  int id; // id number for consumer to distinguish between many producers

protected:
  int onXmppMessage(std::string msg, std::string sender = "") {
    if (pConsumer != NULL)
      return pConsumer->onXmppMessage(msg, sender, this);
    return -1;
  }

public:
  ADXmppProducer() {
    id = IDGenerator++;
    pConsumer = NULL;
  }
  virtual ~ADXmppProducer() {};
  int attach_callback(ADXmppConsumer *c) {
    // allow only one Consumer to be attached
    if (pConsumer == NULL) {
      pConsumer = c;
      return id;
    } else
      return -1; // some other Consumer has already been attached
  }
  int getID() { return id; }
};
/* ------------------------------------------------------------------------- */
// poll period of recv() in TCP mode: bounds the latency of outbound items
#define ADXMPP_RECV_POLL_US 100000
#define ADXMPP_DEFAULT_MAX_MISSED_PONGS 3
#define ADXMPP_DISCONNECT_WAIT_MS 5000
#define ADXMPP_OUTBOUND_QUEUE_MAX 512

// one XMPP account and how to reach it (primary or fallback)
struct XmppAccount {
  std::string user; // full JID
  std::string password;
  std::string server; // optional host override (default: JID domain)
  int port;           // optional port override (default 5222)
  bool useBosh;
  std::string boshUrl;
  std::string boshHost;
  bool tlsVerify;
  std::string saslMech;
  bool tlsEnabled;
  XmppAccount() : port(0), useBosh(false), tlsVerify(true), tlsEnabled(true) {}
  bool configured() const { return !user.empty() && !password.empty(); }
};
#define ADXMPP_PROBE_TIMEOUT_S 30

class ADXmppProxy : public MessageSessionHandler,
                    ConnectionListener,
                    LogHandler,
                    MessageEventHandler,
                    MessageHandler,
                    ChatStateHandler,
                    EventHandler,
                    RosterListener,
                    public ADXmppProducer {
public:
  ADXmppProxy();
  ~ADXmppProxy();
  // Blocks for the whole session; returns when the connection ends.
  int connect(const XmppAccount &account, std::string adminbuddy = "",
              std::string bkupadminbuddy = "");
  // Authenticates with a throwaway client and disconnects again. Used by the
  // fallback logic to probe the primary account while a fallback session is
  // active. Independent gloox Client, safe to run on another thread.
  static bool probe_account(const XmppAccount &account, bool debugLog,
                            int timeout_s = ADXMPP_PROBE_TIMEOUT_S);
  // Ask the session to end from any thread; waits (bounded) until it did.
  int disconnect();
  std::string extractServerFromJID(const std::string &jid);

  // Helper function for BOSH URL parsing
  struct BoshUrlComponents {
    std::string protocol;
    std::string host;
    int port;
    std::string path;
  };
  static BoshUrlComponents parseBoshUrl(const std::string &url);

  // outbound API, safe from any thread (queued)
  int send_reply(std::string reply, std::string sender = "");
  bool SendMessageToBuddy(std::string address, const std::string &body,
                          const std::string &subject = "message");
  void send_client_alive_ping();
  int subscribe_buddy(std::string buddy);
  int unsubscribe_buddy(std::string buddy);
  void set_max_missed_pongs(int misses) { MaxMissedPongs = misses; }

  int receive_request(std::string request, std::string sender);
  bool get_connect_sts() { return connected; };
  void SetDebugLog(bool log);
  const std::string currentDateTime();
  bool getForcedDisconnect() { return DisconnectNow; }
  void setForcedDisconnect() { DisconnectNow = true; }
  bool getOnDemandDisconnect() { return OnDemandDisconnect; }
  void setOnDemandDisconnect(bool flag) { OnDemandDisconnect = flag; }
  // true when the last connect() reached an authenticated session
  bool last_session_authenticated() { return SessionAuthenticated; }
  // true while connect() is executing (session thread busy)
  bool is_session_running() { return SessionRunning; }

  // roster mirror queries, safe from any thread
  int get_buddy_list(std::string &returnval);
  bool get_connected_status();
  int get_buddy_online_state(std::string buddy);
  bool IsMyBuddy(std::string buddyaddress);

  virtual void handleEvent(const Event &event); // = 0;
  virtual void onConnect();
  virtual void onDisconnect(ConnectionError e);
  virtual bool onTLSConnect(const CertInfo &info);
  virtual void handleMessage(const Message &msg, MessageSession * /*session*/);
  virtual void handleMessageEvent(const JID &from, MessageEventType event);
  virtual void handleChatState(const JID &from, ChatStateType state);
  virtual void handleMessageSession(MessageSession *session);
  virtual void handleLog(LogLevel level, LogArea area,
                         const std::string &message);

  // roster related overrider
  virtual void onResourceBindError(ResourceBindError error);
  virtual void onSessionCreateError(SessionCreateError error);
  virtual void handleItemSubscribed(const JID &jid);
  virtual void handleItemAdded(const JID &jid);
  virtual void handleItemUnsubscribed(const JID &jid);
  virtual void handleItemRemoved(const JID &jid);
  virtual void handleItemUpdated(const JID &jid);
  virtual void handleRoster(const Roster &roster);
  virtual void handleRosterError(const IQ & /*iq*/);
  virtual void handleRosterPresence(const RosterItem &item,
                                    const std::string &resource,
                                    Presence::PresenceType presence,
                                    const std::string & /*msg*/);
  virtual void handleSelfPresence(const RosterItem &item,
                                  const std::string &resource,
                                  Presence::PresenceType presence,
                                  const std::string & /*msg*/);
  virtual bool handleSubscriptionRequest(const JID &jid,
                                         const std::string & /*msg*/);
  virtual bool handleUnsubscriptionRequest(const JID &jid,
                                           const std::string & /*msg*/);
  virtual void handleNonrosterPresence(const Presence &presence);

  std::string convert_presence_enum_to_str(Presence::PresenceType presence);
  int accept_buddy(std::string buddy); // add to accept list
  int remove_buddy(std::string buddy); // remove from the accept list
  bool is_admin_user(std::string user);
  int proc_cmd_send_message(
      std::string to, std::string message,
      std::string subject); // used for sending messages to other clients
  int get_accept_buddy_list(
      std::string &returnval); // accept_buddy list filled/removed by
                               // accept_buddy/remove_buddy calls

private:
  struct OutItem {
    enum Type { MSG, PING, SUBSCRIBE, UNSUBSCRIBE } type;
    std::string to;
    std::string body;
    std::string subject;
    OutItem(Type t, const std::string &to_ = "", const std::string &body_ = "",
            const std::string &subject_ = "")
        : type(t), to(to_), body(body_), subject(subject_) {}
  };
  std::mutex outMutex;
  std::deque<OutItem> outQueue;
  std::mutex clientMutex; // guards j lifecycle and BOSH-mode direct sends
  std::mutex rosterMutex;
  std::set<std::string> rosterJids;         // bare JIDs in the roster
  std::map<std::string, bool> rosterOnline; // bare JID -> any resource online
  std::mutex acceptMutex;
  vector<std::string>
      AcceptBuddyList; // Accept these buddies if requested by admin

  int enqueue(const OutItem &item);
  void
  flush_outbound(); // gloox-thread context (or under clientMutex in BOSH mode)
  void perform(const OutItem &item); // requires j != NULL, gloox-thread context
  void mirror_roster_from(const Roster &roster);
  // creates and configures a gloox Client (TCP or BOSH stack) for account;
  // shared by connect() and probe_account()
  static Client *create_client(const XmppAccount &account, bool debugLog);

  std::atomic<bool> iConnect; // shows status of onConnect/onDisconnect
  std::atomic<bool> DisconnectNow;
  std::atomic<bool> OnDemandDisconnect;
  std::atomic<bool> DisconnectRequested;
  std::atomic<bool> SessionAuthenticated;
  std::atomic<bool> SessionRunning;
  std::atomic<bool> connected;
  bool DebugLog;
  bool failed_authorization;
  int HeartBeat; // pings without pong, gloox-thread only
  int MaxMissedPongs;
  Client *j;

  // BOSH connection parameters
  std::atomic<bool> UseBOSH; // read by enqueue() on any thread
  std::string BoshURL;
  std::string BoshHost;
  bool TlsVerify;
  std::string SaslMech; // SASL mechanism: "scram-sha-1" or empty for default
  // sessions container
  struct Session {
    gloox::MessageSession *m_session;
    gloox::ChatStateFilter *m_chatStateFilter;
    gloox::MessageEventFilter *m_messageEventFilter;
  };
  typedef std::map<std::string, Session> Sessions;
  Sessions mySessions;
  std::string AdminBuddy;     // guarded by rosterMutex (set per session,
  std::string BkupAdminBuddy; // read by the worker thread)
};
#endif
