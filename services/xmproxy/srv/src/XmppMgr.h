#ifndef __XMPP_MGR_H_
#define __XMPP_MGR_H_

#ifdef USE_CXMPP_LIB
#include "CXmppProxy.hpp"
#else
#include "ADXmppProxy.hpp"
#endif
#include "Acl.h"
#include "Manifest.h"
#include "Subscriptions.h"
#include "XmLog.h"
#include <atomic>
#include <deque>
#include <iostream>
#include <mutex>
#include <time.h>
#include <vector>
#ifdef USE_AI_BOT
#define CPPHTTPLIB_NO_SSL
#define CPPHTTPLIB_NO_COMPRESSION
#include <httplib.h>
#endif
#include "ADCommon.hpp"
#include "ADThread.hpp"
#include "ADTimer.hpp"

using namespace std;
#define CLIENT_ALIVE_PING_DURATION_MS 90000; // 90seconds (default pinginterval)
// connection and queue tuning defaults (override via login file keys)
#define XMPP_DEFAULT_PING_INTERVAL_SEC 90
#define XMPP_DEFAULT_PING_MISSES 3
#define XMPP_DEFAULT_RECONNECT_MIN_SEC 2
#define XMPP_DEFAULT_RECONNECT_MAX_SEC 60
#define XMPP_DEFAULT_ASYNC_TIMEOUT_SEC 300
#define XMPP_MAX_PENDING_CMDS 64 // bounded command queue (P1)
#define XMPP_MAX_SLEEP_SEC 30    // cap for the sleep command (P1)
#define XMPP_DEFAULT_FALLBACK_AFTER                                            \
  3 // consecutive failures before switching account
#define XMPP_DEFAULT_PRIMARY_PROBE_SEC                                         \
  300 // probe period for the primary while on fallback
#define XMPP_DEFAULT_HEARTBEAT_SEC 300 // heartbeat topic period, 0 disables
#define GITHUB_FMW_DOWNLOAD_FOLDER                                             \
  "http://github.com/hackboxguy/downloads/raw/master/"
// #define BRBOX_SYS_CONFIG_FILE_PATH "/boot/sysconfig.txt"
#define BRBOX_SYS_CONFIG_FILE_PATH "/etc/sysconfig.txt"
#define BBOXSMS_SERVER_ADDR "127.0.0.1"
// #define EXMPP_CMD_TABL
// {"smsdeleteall","smsdelete","smsget","smssend","smsupdate","smstotal","fmwver","fmwupdt","fmwupsts","fmwupres","reboot","uptime","hostname","myip","resethostname","dialvoice","dialussd","readussd","logsts","gsmcheck","logupdate","logcount","logmsg","unknown","none","\0"}
// #define EXMPP_CMD_TABL_HELP
// {"","<zero_based_index>","<zero_based_index>","<phone-num>
// <msg>","","","","<filename>","","","","","","","","<phone-num>","<ussd-code>","","<port>
// [sts]","","","","<index>","unknown","none","\0"}

typedef enum EXMPP_CMD_TYPES_T {
  EXMPP_CMD_SMS_DELETE_ALL = 0,
  EXMPP_CMD_SMS_DELETE,
  EXMPP_CMD_SMS_GET,
  EXMPP_CMD_SMS_SEND,
  EXMPP_CMD_SMS_LIST_UPDATE,
  EXMPP_CMD_SMS_GET_TOTAL,
  EXMPP_CMD_FMW_GET_VERSION,
  EXMPP_CMD_FMW_UPDATE,
  EXMPP_CMD_FMW_UPDATE_STS,
  EXMPP_CMD_FMW_UPDATE_RES, // result of the last fmw update command
  EXMPP_CMD_FMW_REBOOT,
  EXMPP_CMD_FMW_UPTIME,
  // EXMPP_CMD_FMW_GET_HOSTNAME,
  // EXMPP_CMD_FMW_SET_HOSTNAME,
  EXMPP_CMD_FMW_HOSTNAME,
  EXMPP_CMD_FMW_GET_MYIP,
  EXMPP_CMD_FMW_RESET_HOSTNAME,
  EXMPP_CMD_DIAL_VOICE,
  EXMPP_CMD_DIAL_USSD,
  EXMPP_CMD_GET_USSD,
  EXMPP_CMD_DEBUG_LOG_STS,
  EXMPP_CMD_GSM_MODEM_IDENT, // identify gsm-usb modem
  EXMPP_CMD_LOG_UPDATE,      // trigger reloading of log message to vector list
  EXMPP_CMD_LOG_COUNT,       // read total items in vector list
  EXMPP_CMD_LOG_MSG,         // read item-msg from vector list
  EXMPP_CMD_FMW_GET_LOCALIP,
  EXMPP_CMD_FMW_POWEROFF,      // shutdown linux and dont reboot
  EXMPP_CMD_GPIO,              // gpio pin read write
  EXMPP_CMD_GSM_EVENT_NOTIFY,  // sms/call async-event-notification
  EXMPP_CMD_GPIO_EVENT_NOTIFY, // gpio async-event-notification
  EXMPP_CMD_ALIAS,
  EXMPP_CMD_SLEEP,
  EXMPP_CMD_ACCOUNT,
  EXMPP_CMD_BOTNAME,       // name of the chat-bot, set a friendly name e.g:
                           // myhome-raspi-bot(helps in identifying when u have
                           // multiple bots)
  EXMPP_CMD_BUDDY_LIST,    // returns list of buddies
  EXMPP_CMD_SHELLCMD,      // executes remote shell command and returns the
                           // success/fail value of shell-command
  EXMPP_CMD_SHELLCMD_RESP, // executes remote shell command and returns the
                           // shell-command's text output
  EXMPP_CMD_DEVIDENT,
  EXMPP_CMD_SHUTDOWN, // shuts down the xmproxysrv(xmpp-log-out)
  EXMPP_CMD_SONOFF, // http based control of sonoff relay with tasmota firmware
  EXMPP_CMD_DISPCLEAR, // clear-display
  EXMPP_CMD_DISPPRINT, // display-print
  EXMPP_CMD_DISPBKLT,  // display-backlight-control
  EXMPP_CMD_SHELLCMD_TRIG,
  EXMPP_CMD_BUDDY_ADD,
  EXMPP_CMD_BUDDY_REMOVE,
  EXMPP_CMD_BUDDY_SUBSCRIBE,
  EXMPP_CMD_BUDDY_UNSUBSCRIBE,
  EXMPP_CMD_ACCEPT_BUDDY_LIST,
  EXMPP_CMD_RELAY_MESSAGE,
  EXMPP_CMD_ACL,      // list/set/remove/reload buddy roles (admin)
  EXMPP_CMD_MANIFEST, // manifest summary (viewer), reload/check (admin)
  EXMPP_CMD_RUN,      // run <control-id> [on|off] from the manifest
  EXMPP_CMD_WATCH,    // watch [<topic,...>]: subscribe to events / list
  EXMPP_CMD_UNWATCH,  // unwatch [<topic,...>]
  EXMPP_CMD_UNKNOWN,
  EXMPP_CMD_NONE
} EXMPP_CMD_TYPES;

struct XmppCmdEntry {
  std::string cmdMsg;
  std::string sender;
  bool aiPrompt; // forward to the AI agent instead of the command table
  bool json;     // JSON-RPC 2.0 body (bucket 4)
  std::string pollControl;   // poll job for this manifest control (bucket 6)
  std::string pollInitialTo; // deliver the value to this jid even if unchanged

public:
  XmppCmdEntry(std::string msg, std::string from, bool ai = false,
               bool js = false)
      : cmdMsg(msg), sender(from), aiPrompt(ai), json(js) {}
};
/* ------------------------------------------------------------------------- */
#define EXMPP_EVNT_TYPES_TABL {"gsm", "gpio", "unknown", "none", "\0"}
typedef enum EXMPP_EVNT_TYPES_T {
  EXMPP_EVNT_GSM = 0,
  EXMPP_EVNT_GPIO,
  EXMPP_EVNT_UNKNOWN,
  EXMPP_EVNT_NONE
} EXMPP_EVNT_TYPES;
/* ------------------------------------------------------------------------- */
struct AyncEventEntry {
  int taskID;        // async event taskID returned by server
  int srvPort;       // port where async command was sent
  int xmppTID;       // internal global task id of xmpp-proxy
  std::string to;    // reply back to this requestor
  time_t created;    // for the timeout sweep
  bool json;         // requester used JSON-RPC: completion goes as notification
  std::string reqId; // its request id, echoed in the notification
  std::string pollControl;   // completion belongs to a poll of this control
  std::string pollInitialTo; // and its value goes to this jid regardless
  bool shellOutput;          // value is the captured shellcmd output
public:
  AyncEventEntry(int tid, int port, int xmtid, std::string sender)
      : taskID(tid), srvPort(port), xmppTID(xmtid), to(sender),
        created(time(NULL)), json(false), shellOutput(false) {}
};
// following functor object is used as predicator for finding a specific vector
// element entry based on srvToken
class FindAsyncEventEntry {
  const int mytaskID;
  const int mysrvPort;

public:
  FindAsyncEventEntry(const int tid, const int port)
      : mytaskID(tid), mysrvPort(port) {}
  bool operator()(AyncEventEntry pEntry) const {
    if (pEntry.taskID == mytaskID && pEntry.srvPort == mysrvPort)
      return true;
    else
      return false;
  }
};
/* ------------------------------------------------------------------------- */
struct EventSubscrEntry {
  std::string subscriber;
  EXMPP_EVNT_TYPES m_EvntType;
  int m_EvntArg; // e.g:gpio number
  bool m_Status; // enable/disable status
public:
  EventSubscrEntry(std::string subscr, EXMPP_EVNT_TYPES typ, int arg, bool sts)
      : subscriber(subscr), m_EvntType(typ), m_EvntArg(arg), m_Status(sts) {}
};
class FindEventSubscrEntry {
  const std::string Addr;
  const int Arg;
  EXMPP_EVNT_TYPES EvntType;

public:
  FindEventSubscrEntry(const std::string addr, const int arg,
                       EXMPP_EVNT_TYPES type)
      : Addr(addr), Arg(arg), EvntType(type) {}
  bool operator()(EventSubscrEntry pEntry) const {
    if (pEntry.m_EvntType == EXMPP_EVNT_GSM) {
      if (pEntry.subscriber == Addr && pEntry.m_EvntType == EvntType)
        return true;
      else
        return false;
    } else if (pEntry.m_EvntType == EXMPP_EVNT_GPIO) {
      if (pEntry.subscriber == Addr && pEntry.m_EvntArg == Arg &&
          pEntry.m_EvntType == EvntType)
        return true;
      else
        return false;
    } else
      return false;
  }
};
/* ------------------------------------------------------------------------- */
// #define EXMPP_EVNT_TYPES_TABL    {"gsm","gpio","unknown","none","\0"}
typedef enum EXMPP_USER_ACCESS_TYPES_T {
  EXMPP_USER_ACCESS_ADMIN = 0, // allow only for admin user
  EXMPP_USER_ACCESS_READWRITE, // allow read/write
  EXMPP_USER_ACCESS_READONLY,  // allow only for users with limited privilages
  EXMPP_USER_ACCESS_UNKNOWN,
  EXMPP_USER_ACCESS_NONE
} EXMPP_USER_ACCESS_TYPES;
typedef struct XMPROXY_CMD_TABLE_T {
  bool cmdsts;
  EXMPP_CMD_TYPES cmd;
  char cmd_name[128];
  char cmd_arg[128];
  EXMPP_USER_ACCESS_TYPES cmdaccess; // access-level
} XMPROXY_CMD_TABLE;
/* ------------------------------------------------------------------------- */
#ifdef USE_CXMPP_LIB
class XmppMgr : public CXmppConsumer,
                public ADThreadConsumer,
                public ADTimerConsumer
#else
class XmppMgr : public ADXmppConsumer,
                public ADThreadConsumer,
                public ADTimerConsumer
#endif
{
  int XmppTaskIDCounter;
  int heartbeat_ms;
  int event_period_ms; //
  int CyclicTime_ms;   // keepalive ping period
  int sweep_period_ms; // async-task timeout sweep accumulator
  int LastFmwUpdateTaskID;
  // connection tuning (login file keys pinginterval, pingmisses,
  // reconnectmin, reconnectmax, asynctimeout)
  int PingIntervalSec;
  int PingMisses;
  int ReconnectMinSec;
  int ReconnectMaxSec;
  int AsyncTimeoutSec;
  std::mutex cmdMutex;   // processCmd
  std::mutex inboxMutex; // Inbox, ResponseMsg
  std::mutex asyncMutex; // AsyncTaskList
  void sweep_async_tasks();

  ADTimer *pMyTimer;
  bool DebugLog;
  std::string bboxSmsServerAddr;
  std::deque<XmppCmdEntry> processCmd; // fifo for processing xmpp messages
  std::vector<AyncEventEntry> AsyncTaskList;
  std::deque<std::string> Inbox;
  std::string XmppUserName;
  std::string XmppUserPw;
  // main roster-contact as admin for remote management
  std::string XmppAdminBuddy;
  std::string XmppBkupAdminBuddy; // backup admin buddy
  std::string XmppBotName;
  std::string XmppBotNameFilePath;
  std::string XmppNetInterface;
  std::string AiAgentUrl;
  std::string AiModel;
  // BOSH connection parameters (for corporate networks)
  bool XmppUseBosh;
  std::string XmppBoshUrl;
  std::string XmppBoshHost;
  bool XmppTlsVerify;
  bool XmppTlsEnabled; // Controls XMPP-level TLS (STARTTLS) - disable for HTTP
                       // BOSH with external TLS proxy
  std::string
      XmppSaslMech; // SASL mechanism: "scram-sha-1" or empty for default
#ifdef USE_CXMPP_LIB
  CXmppProxy XmppProxy; // xmpp client
#else
  ADXmppProxy XmppProxy; // xmpp client
#endif
  // primary and optional fallback account (login file keys with the
  // "fallback" prefix). After FallbackAfter consecutive failures the session
  // loop switches account; while on the fallback, ProbeThread probes the
  // primary every PrimaryProbeSec and triggers the switch back.
  XmppAccount PrimaryAccount;
  XmppAccount FallbackAccount;
  int FallbackAfter;
  int PrimaryProbeSec;
  std::atomic<bool> OnFallback;
  std::atomic<bool> PrimaryAvailable; // set by the probe thread
  std::atomic<bool> ProbeStop;
  std::atomic<bool> ProbeRunning;
  std::atomic<bool> Stopping; // set by Stop(): heartbeats are ignored
  ADThread ProbeThread;
  int clientThreadID;
  int probeThreadID;
  std::mutex activeMutex;
  std::string ActiveJid;
  XmAcl Acl;
  std::string AclFile;
  XmManifestStore Manifest;
  std::string ManifestFile;
  // one executed step of a control or exec
  struct StepResult {
    std::string cmd;
    RPC_SRV_RESULT res;
    std::string text;
    int task;
  };
  // runs a manifest control for sender; checks the control's role (the
  // manifest grant) and then executes its command(s) with that grant
  RPC_SRV_RESULT execute_control(const XmControl &control,
                                 const std::string &arg,
                                 const std::string &sender, XM_ROLE senderRole,
                                 std::vector<StepResult> &steps,
                                 std::string &errorText);
  std::vector<std::string> manifest_command_warnings();
  RPC_SRV_RESULT proc_cmd_manifest(std::string msg, std::string &returnval,
                                   XM_ROLE senderRole);
  RPC_SRV_RESULT proc_cmd_run(std::string msg, std::string &returnval,
                              const std::string &sender, XM_ROLE senderRole);
  std::string AppVersion; // reported by describe
  // request context while the worker runs a command (worker thread only):
  // async task entries created meanwhile are tagged with it
  struct ReqCtx {
    bool json;
    std::string reqId;
    std::string pollControl;
    std::string pollInitialTo;
    bool shellOutput;
    ReqCtx() : json(false), shellOutput(false) {}
    void clear() {
      json = false;
      reqId.clear();
      pollControl.clear();
      pollInitialTo.clear();
      shellOutput = false;
    }
  } CurrentReq;
  // ---- events (bucket 6, XmppEvents.cpp) ----
  XmSubscriptions Subs;
  std::string SubscrFile;
  int HeartbeatSec;
  int heartbeat_period_ms;
  time_t StartedAt;
  ADThread PollerThread;
  int pollerThreadID;
  std::atomic<bool> PollerStop;
  std::atomic<bool> PollerRunning;
  struct ControlState {
    bool valid;
    std::string value;
    std::string error;
    time_t lastPoll;
    bool inflight;
    ControlState() : valid(false), lastPoll(0), inflight(false) {}
  };
  std::map<std::string, ControlState> controlStates;
  std::mutex stateMutex;
  std::deque<std::string> pendingSystemEvents;
  std::mutex systemMutex;
  bool wasConnected;
  int poll_loop();
  void enqueue_poll(const std::string &id, const std::string &initialTo);
  void poll_control(const std::string &id, const std::string &initialTo);
  void finish_poll(const std::string &id, const std::string &initialTo,
                   RPC_SRV_RESULT res, const std::string &raw);
  bool extract_value(const XmControl &c, const std::string &raw,
                     std::string &value);
  void publish(const std::string &topic, struct json_object *params,
               const std::string &exceptJid = "");
  void publish_to(const std::string &jid, const std::string &topic,
                  struct json_object *params);
  bool topic_valid(const std::string &topic, XM_ROLE role, std::string &why,
                   int *errCode);
  void request_initial_polls(const std::string &jid,
                             const std::set<std::string> &topics);
  void set_pending_system_event(const std::string &ev);
  void emit_system_events();
  RPC_SRV_RESULT proc_cmd_watch(std::string msg, std::string &returnval,
                                const std::string &sender, XM_ROLE role);
  RPC_SRV_RESULT proc_cmd_unwatch(std::string msg, std::string &returnval,
                                  const std::string &sender);
  std::deque<std::string> expand_command(std::string msg);
  RPC_SRV_RESULT run_single_command(const std::string &cmd,
                                    const std::string &sender,
                                    XM_ROLE senderRole, std::string &returnval,
                                    EXMPP_CMD_TYPES *typeOut);
  std::string result_code_name(RPC_SRV_RESULT res);
  // JSON-RPC 2.0 over XMPP (XmppJson.cpp)
  void process_json_request(const XmppCmdEntry &entry);
  std::string json_busy_response();
  void send_task_notification(const std::string &to, int taskID,
                              const std::string &result,
                              const std::string &reqId);
  // duplicate suppression (P3): recent (sender, id) -> response
  struct RecentReply {
    std::string sender;
    std::string id;
    std::string response;
    time_t when;
  };
  std::deque<RecentReply> recentReplies;
  std::mutex recentMutex;

public:
  struct json_object *json_describe(XM_ROLE role);
  struct json_object *json_subscribe(struct json_object *params,
                                     const std::string &sender, XM_ROLE role,
                                     struct json_object **error);
  struct json_object *json_unsubscribe(struct json_object *params,
                                       const std::string &sender,
                                       struct json_object **error);
  struct json_object *json_get_subscriptions(const std::string &sender);
  struct json_object *json_get_manifest(XM_ROLE role,
                                        struct json_object **error);
  struct json_object *json_exec_control(const std::string &id,
                                        const std::string &arg,
                                        const std::string &sender, XM_ROLE role,
                                        const std::string &reqId,
                                        struct json_object **error);
  bool manifest_loaded() { return Manifest.is_loaded(); }
  struct json_object *json_list_commands(XM_ROLE role);
  struct json_object *json_exec(const std::string &cmd,
                                const std::string &sender, XM_ROLE role,
                                const std::string &reqId,
                                struct json_object **error);
  bool find_recent_reply(const std::string &sender, const std::string &id,
                         std::string &response);
  void remember_reply(const std::string &sender, const std::string &id,
                      const std::string &response);
  XM_ROLE required_role_public(EXMPP_CMD_TYPES cmd, const std::string &msg) {
    return required_role(cmd, msg);
  }

private:
  // role of an authorized sender: admin buddies are always admin, roster
  // members get their ACL entry or the default role
  XM_ROLE role_of_sender(const std::string &sender);
  // minimum role for a command; some commands depend on their arguments
  // (reading is viewer, changing is admin or operator)
  XM_ROLE required_role(EXMPP_CMD_TYPES cmd, const std::string &msg);
  RPC_SRV_RESULT proc_cmd_acl(std::string msg, std::string &returnval);
  void set_active_jid(const std::string &jid);
  int session_loop();
  int probe_loop();

  std::string AliasListFile;
  typedef std::map<std::string, std::string> Alias;
  Alias AliasList;
  std::string UpdateUrlFile;
#ifdef USE_AI_BOT
  httplib::Client *botcli;
#endif
  // struct EventSubscription
  //{
  //	std::string subscriber;
  //	EXMPP_EVNT_TYPES m_EvntType;
  //	int m_EvntArg;//e.g:gpio number
  //	bool m_Status;//enable/disable status
  // };
  // typedef std::vector<EventSubscription> EvntSubscr;
  // typedef std::map<std::string, EventSubscription> EvntSubscr;
  // EvntSubscr myEventList;
  std::string EventSubscrListFile;
  std::vector<EventSubscrEntry> myEventList;

  // xmpp-client-callback functions
#ifdef USE_CXMPP_LIB
  virtual int onXmppMessage(std::string msg, std::string sender,
                            CXmppProducer *pObj);
#else
  virtual int onXmppMessage(std::string msg, std::string sender,
                            ADXmppProducer *pObj);
#endif

  ADThread XmppClientThread,
      XmppCmdProcessThread; // thread for xmpp client connection
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj); //{return 0;};
  virtual int thread_callback_function(
      void *pUserData,
      ADThreadProducer *pObj); //{return 0;};//we are not using this one..

  // ADTimerConsumer overrides: 100ms timer and sigio
  virtual int sigio_notification() { return 0; };
  virtual int timer_notification(); //{return 0;};
  virtual int custom_sig_notification(int signum) { return 0; };

  EXMPP_CMD_TYPES ResolveCmdStr(std::string cmd);
  RPC_SRV_RESULT proc_cmd_sms_deleteall(std::string msg, std::string &returnval,
                                        std::string sender);
  RPC_SRV_RESULT proc_cmd_sms_delete(std::string msg);
  RPC_SRV_RESULT proc_cmd_sms_get(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_sms_send(std::string msg, std::string &returnval,
                                   std::string sender);
  RPC_SRV_RESULT proc_cmd_sms_list_update(std::string msg,
                                          std::string &returnval,
                                          std::string sender);
  RPC_SRV_RESULT proc_cmd_sms_get_total(std::string msg,
                                        std::string &returnval);
  RPC_SRV_RESULT proc_cmd_fmw_get_version(std::string msg,
                                          std::string &returnval);
  RPC_SRV_RESULT proc_cmd_fmw_update(std::string msg, std::string &returnval,
                                     std::string sender);
  RPC_SRV_RESULT proc_cmd_fmw_reboot(std::string msg, std::string &returnval,
                                     std::string sender);
  RPC_SRV_RESULT proc_cmd_fmw_poweroff(std::string msg, std::string &returnval,
                                       std::string sender);
  RPC_SRV_RESULT proc_cmd_fmw_update_sts(std::string msg,
                                         std::string &returnval);
  RPC_SRV_RESULT proc_cmd_fmw_update_res(std::string msg,
                                         std::string &returnval);
  RPC_SRV_RESULT proc_cmd_fmw_uptime(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_fmw_get_hostname(std::string msg,
                                           std::string &returnval);
  RPC_SRV_RESULT proc_cmd_fmw_set_hostname(std::string msg);
  RPC_SRV_RESULT proc_cmd_fmw_hostname(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_fmw_get_myip(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_fmw_set_default_hostname(std::string msg);
  RPC_SRV_RESULT proc_cmd_dial_voice(std::string msg, std::string &returnval,
                                     char *rpc_cmd, std::string sender);
  RPC_SRV_RESULT proc_cmd_get_ussd(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_logsts(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_gsm_modem_identify(std::string msg,
                                             std::string &returnval,
                                             std::string sender);
  RPC_SRV_RESULT proc_cmd_log_list_update(std::string msg,
                                          std::string &returnval,
                                          std::string sender);
  RPC_SRV_RESULT proc_cmd_log_get_count(std::string msg,
                                        std::string &returnval);
  RPC_SRV_RESULT proc_cmd_log_get_line(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_fmw_get_localip(std::string msg,
                                          std::string &returnval);
  RPC_SRV_RESULT proc_cmd_gpio(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_event_gsm(std::string msg, std::string sender,
                                    std::string &returnval);
  RPC_SRV_RESULT proc_cmd_event_gpio(std::string msg, std::string sender,
                                     std::string &returnval);
  RPC_SRV_RESULT proc_cmd_alias(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_sleep(std::string msg);
  RPC_SRV_RESULT proc_cmd_account_name(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_bot_name(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_buddy_list(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_shellcmd(std::string msg, std::string &returnval,
                                   std::string sender, EXMPP_CMD_TYPES cmdtype);
  RPC_SRV_RESULT proc_cmd_shellcmdresp(std::string msg, std::string &returnval,
                                       std::string sender);
  RPC_SRV_RESULT proc_cmd_devident(std::string msg, std::string &returnval,
                                   std::string sender);
  RPC_SRV_RESULT proc_cmd_xmpshutdown(std::string msg, std::string &returnval,
                                      std::string sender);
  RPC_SRV_RESULT proc_cmd_sonoff(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_disp_clear(
      std::string msg); //,std::string &returnval,std::string sender);
  RPC_SRV_RESULT proc_cmd_disp_print(std::string msg, std::string &returnval);
  RPC_SRV_RESULT proc_cmd_disp_backlight(std::string msg,
                                         std::string &returnval);
  RPC_SRV_RESULT proc_cmd_get_display_backlight(std::string msg,
                                                std::string &returnval);
  RPC_SRV_RESULT proc_cmd_set_display_backlight(std::string msg);
  RPC_SRV_RESULT proc_cmd_buddy_add(std::string msg, std::string &returnval,
                                    std::string sender);
  RPC_SRV_RESULT proc_cmd_buddy_remove(std::string msg, std::string &returnval,
                                       std::string sender);
  RPC_SRV_RESULT proc_cmd_buddy_subscribe(std::string msg,
                                          std::string &returnval,
                                          std::string sender);
  RPC_SRV_RESULT proc_cmd_buddy_unsubscribe(std::string msg,
                                            std::string &returnval,
                                            std::string sender);
  RPC_SRV_RESULT proc_cmd_accept_buddy_list(std::string msg,
                                            std::string &returnval);
  RPC_SRV_RESULT proc_cmd_relay_message(std::string msg, std::string &returnval,
                                        std::string sender);

  std::string print_help(XM_ROLE role);
  RPC_SRV_RESULT LoadAliasList(std::string listFile);
  RPC_SRV_RESULT ExtendAliasList(std::string listFile, std::string key,
                                 std::string val);
  RPC_SRV_RESULT RewriteAliasList(std::string listFile);

  RPC_SRV_RESULT LoadEventSubscrList(std::string listFile,
                                     std::vector<EventSubscrEntry> *pList);
  RPC_SRV_RESULT ExtendEventSubscrList(std::string listFile, std::string addr,
                                       EXMPP_EVNT_TYPES type, int arg);
  RPC_SRV_RESULT RewriteEventSubscrList(std::string listFile,
                                        std::vector<EventSubscrEntry> *pList);
  RPC_SRV_RESULT hostname_to_ip(char *hostname, char *ip);
  std::string generate_ai_response(std::string &prompt);

public:
  XmppMgr();
  ~XmppMgr();
  RPC_SRV_RESULT Start(std::string accountFilePath);
  RPC_SRV_RESULT Stop();
  RPC_SRV_RESULT SendMessage(std::string msg);
  void SetDebugLog(bool log);
  void SetAiAgentUrl(std::string url);
  void SetAiModel(std::string model);
  int AttachHeartBeat(ADTimer *pTimer);
  RPC_SRV_RESULT RpcResponseCallback(RPC_SRV_RESULT taskRes, int taskID,
                                     std::string to, bool json = false,
                                     std::string reqId = "",
                                     std::string pollControl = "",
                                     std::string pollInitialTo = "",
                                     bool shellOutput = false);
  RPC_SRV_RESULT RpcResponseCallback(std::string taskRes, int taskID,
                                     std::string to, bool json = false,
                                     std::string reqId = "",
                                     std::string pollControl = "",
                                     std::string pollInitialTo = "",
                                     bool shellOutput = false);
  RPC_SRV_RESULT GpioEventCallback(int evntNum, int evntArg);
  // RPC_SRV_RESULT IsItMyAsyncTaskResp(int tid,int port);
  RPC_SRV_RESULT AccessAsyncTaskList(int tid, int port, bool insertEntryFlag,
                                     int *xmpptID, std::string &sender,
                                     bool *json = NULL,
                                     std::string *reqId = NULL,
                                     std::string *pollControl = NULL,
                                     std::string *pollInitialTo = NULL,
                                     bool *shellOutput = NULL);
  void SetUSBGsmSts(bool sts);
  void SetOpenWrtCmdGroupSts(bool sts);
  void SetDockerCmdGroupSts(bool sts);
  inline void SetAliasListFilePath(std::string filepath) {
    AliasListFile = filepath;
  };
  inline void SetEventSubscrListFilePath(std::string filepath) {
    EventSubscrListFile = filepath;
  };
  inline void SetAclFilePath(std::string filepath) { AclFile = filepath; };
  inline void SetManifestFilePath(std::string filepath) {
    ManifestFile = filepath;
  };
  inline void SetSubscrFilePath(std::string filepath) {
    SubscrFile = filepath;
  };
  inline void SetUpdateurlFilePath(std::string filepath) {
    UpdateUrlFile = filepath;
  };
  void SetBotNameFilePath(std::string filepath);
  void SetVersion(std::string v) { AppVersion = v; }
  RPC_SRV_RESULT xpandarg(std::string &cmdArg);
  RPC_SRV_RESULT xpandargs(std::string &cmdArg);
  inline void SetNetInterface(std::string interface) {
    XmppNetInterface = interface;
  };
  inline bool get_connected_status() {
    return XmppProxy.get_connected_status();
  };
  std::string get_active_jid();
  std::string get_primary_jid() { return PrimaryAccount.user; }
  std::string get_fallback_jid() { return FallbackAccount.user; }
  bool is_on_fallback() { return OnFallback; }
  RPC_SRV_RESULT
  set_online_status(bool status); // user set online/offline status via rpc
  RPC_SRV_RESULT proc_cmd_send_message(
      std::string to, std::string message,
      std::string subject =
          "message"); // used for sending message to other clients
  RPC_SRV_RESULT proc_cmd_send_message_internal(
      std::string to, std::string message,
      std::string subject =
          "message"); // used for sending message to other clients

  RPC_SRV_RESULT proc_cmd_subscribe_message(
      std::string to, std::string message = "hi",
      std::string subject = "message"); // used for sending friend request
  RPC_SRV_RESULT proc_cmd_add_buddy(
      std::string to, std::string message = "hi",
      std::string subject = "message"); // used for accepting buddy-request
  std::string ResponseMsg;
  RPC_SRV_RESULT proc_cmd_get_inbox_count(int &count);
  RPC_SRV_RESULT proc_cmd_get_inbox_msg(int index, std::string &message);
  RPC_SRV_RESULT proc_cmd_get_inbox_empty();
};
#endif
