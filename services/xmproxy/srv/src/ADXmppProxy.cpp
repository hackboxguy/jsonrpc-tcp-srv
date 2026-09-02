#include "ADXmppProxy.hpp"
#include "XmLog.h"
#include <iostream>
using namespace std;
#include <stdio.h>
#include <string>
#include <time.h>
#include <unistd.h>

/*****************************************************************************/
int ADXmppProducer::IDGenerator =
    0; // generate Unique ID for every ADXmppProxy object
/*****************************************************************************/
ADXmppProxy::ADXmppProxy()
    : iConnect(false), DisconnectNow(false), OnDemandDisconnect(false),
      DisconnectRequested(false), SessionAuthenticated(false),
      SessionRunning(false), connected(false) {
  DebugLog = false;
  failed_authorization = false;
  HeartBeat = 0;
  MaxMissedPongs = ADXMPP_DEFAULT_MAX_MISSED_PONGS;
  j = NULL;
  AdminBuddy = "";
  BkupAdminBuddy = "";
  // BOSH parameters
  UseBOSH = false;
  BoshURL = "";
  BoshHost = "";
  TlsVerify = true;
  SaslMech = "";
}
ADXmppProxy::~ADXmppProxy() { mySessions.clear(); }
/*****************************************************************************/
// Request the running session to end. TCP mode: the session thread notices
// DisconnectRequested on its next recv() poll and calls gloox from there.
// BOSH mode: recv() blocks, so gloox::disconnect is called here under
// clientMutex (documented BOSH limitation). Waits up to
// ADXMPP_DISCONNECT_WAIT_MS for the session to end.
int ADXmppProxy::disconnect() {
  DisconnectRequested = true;
  {
    std::lock_guard<std::mutex> lock(clientMutex);
    if (j != NULL && UseBOSH)
      j->disconnect();
  }
  int waited = 0;
  while (connected && waited < ADXMPP_DISCONNECT_WAIT_MS) {
    usleep(100000); // 100ms
    waited += 100;
  }
  if (connected)
    XMLOG_WRN("xmpp: timeout waiting for session to end (%d ms)", waited);
  return 0;
}
// Function to extract the server part from a JID
std::string ADXmppProxy::extractServerFromJID(const std::string &jid) {
  size_t atPos = jid.find('@');
  if (atPos == std::string::npos) {
    // If no '@' is found, assume the entire string is the server
    return jid;
  }
  // Extract the server part after the '@'
  return jid.substr(atPos + 1);
}
/*****************************************************************************/
// Helper function to parse BOSH URL into components
ADXmppProxy::BoshUrlComponents
ADXmppProxy::parseBoshUrl(const std::string &url) {
  BoshUrlComponents result;
  size_t pos = 0;

  // Extract protocol (https:// or http://)
  size_t protocolEnd = url.find("://");
  if (protocolEnd != std::string::npos) {
    result.protocol = url.substr(0, protocolEnd);
    pos = protocolEnd + 3;
  } else {
    result.protocol = "http";
  }

  // Find path start
  size_t pathStart = url.find("/", pos);
  std::string hostPort;

  if (pathStart != std::string::npos) {
    hostPort = url.substr(pos, pathStart - pos);
    result.path = url.substr(pathStart);
  } else {
    hostPort = url.substr(pos);
    result.path = "/";
  }

  // Split host:port (use rfind to handle IPv6 addresses)
  size_t portPos = hostPort.rfind(":");
  if (portPos != std::string::npos) {
    result.host = hostPort.substr(0, portPos);
    result.port = std::stoi(hostPort.substr(portPos + 1));
  } else {
    result.host = hostPort;
    result.port = (result.protocol == "https") ? 443 : 80;
  }

  return result;
}
/*****************************************************************************/
int ADXmppProxy::connect(char *user, char *password, std::string adminbuddy,
                         std::string bkupadminbuddy, bool useBosh,
                         std::string boshUrl, std::string boshHost,
                         bool tlsVerify, std::string saslMech,
                         bool tlsEnabled) {
  if (j != NULL)
    return 0;

  SessionRunning = true;
  SessionAuthenticated = false;
  DisconnectRequested = false;
  if (DebugLog)
    cout << "ADXmppProxy::connect: Entering===>" << endl;

  {
    std::lock_guard<std::mutex> lock(rosterMutex);
    AdminBuddy = adminbuddy;
    BkupAdminBuddy = bkupadminbuddy;
  }
  UseBOSH = useBosh;
  BoshURL = boshUrl;
  BoshHost = boshHost;
  TlsVerify = tlsVerify;
  SaslMech = saslMech;

  std::string server = extractServerFromJID(user);

  // If boshHost not provided, use server from JID
  if (UseBOSH && BoshHost.empty()) {
    BoshHost = server;
  }

  JID jid(user);
  // Note: Don't explicitly set server - let gloox parse it from the full JID
  // jid.setServer(server) can cause SCRAM authentication issues
  if (DebugLog)
    cout << "ADXmppProxy::connect: Using JID: " << jid.full()
         << " (server: " << server << ")" << endl;

  {
    std::lock_guard<std::mutex> lock(clientMutex);
    j = new Client(jid, password);
  }
  connected = true; // after creation of Client object, make this flag true

  // Configure SASL mechanisms if specified
  if (!SaslMech.empty()) {
    if (SaslMech == "scram-sha-1") {
      // Disable SCRAM-SHA-1-PLUS to avoid channel binding issues
      // Use only SCRAM-SHA-1 (without channel binding)
      j->setSASLMechanisms(SaslMechAll ^ SaslMechScramSha1Plus);
      cout << "ADXmppProxy::connect: SASL mechanism set to SCRAM-SHA-1 "
              "(channel binding disabled)"
           << endl;
    } else if (SaslMech == "scram-sha-1-plus") {
      // Use only SCRAM-SHA-1-PLUS (with channel binding)
      j->setSASLMechanisms(SaslMechScramSha1Plus);
      cout << "ADXmppProxy::connect: SASL mechanism set to SCRAM-SHA-1-PLUS "
              "(channel binding enabled)"
           << endl;
    } else if (SaslMech == "plain") {
      // Use PLAIN mechanism (simple base64-encoded username/password)
      // Useful for testing credentials, but sends password in cleartext over
      // TLS
      j->setSASLMechanisms(SaslMechPlain);
      cout << "ADXmppProxy::connect: SASL mechanism set to PLAIN (credentials "
              "sent over TLS)"
           << endl;
    } else {
      cout << "ADXmppProxy::connect: Warning - Unknown SASL mechanism: "
           << SaslMech << ", using default" << endl;
    }
  } else {
    if (DebugLog)
      cout << "ADXmppProxy::connect: Using default SASL mechanisms" << endl;
  }

  if (UseBOSH) {
    // BOSH mode - tunnel XMPP over HTTP(S)
    if (DebugLog || true) { // Always log BOSH attempts
      cout << "ADXmppProxy::connect: ========== BOSH MODE ENABLED =========="
           << endl;
      cout << "  BOSH URL:      " << BoshURL << endl;
      cout << "  BOSH Host:     " << BoshHost << endl;
      cout << "  TLS Verify:    " << (TlsVerify ? "true" : "false") << endl;
    }

    // Parse BOSH URL into components
    BoshUrlComponents urlParts = parseBoshUrl(BoshURL);

    if (DebugLog || true) {
      cout << "  Parsed URL:" << endl;
      cout << "    protocol:  " << urlParts.protocol << endl;
      cout << "    host:      " << urlParts.host << endl;
      cout << "    port:      " << urlParts.port << endl;
      cout << "    path:      " << urlParts.path << endl;
    }

    // Set server FIRST (before creating connections) - needed for BOSH
    j->setServer(BoshHost);

    // Disable compression for BOSH
    j->setCompression(false);

    // Create TCP connection to the IP/hostname
    ConnectionTCPClient *conn0 = new ConnectionTCPClient(
        j->logInstance(),
        urlParts.host, // IP address or hostname (e.g., 192.168.1.2)
        urlParts.port  // Usually 443 for HTTPS, 5281 for HTTP proxy
    );

    // For HTTPS: wrap with TLS layer
    // For HTTP: use raw TCP (external proxy like socat handles TLS)
    ConnectionBase *connBase = conn0; // Default to raw TCP

    if (urlParts.protocol == "https") {
      // Wrap TCP with TLS layer for HTTPS
      ConnectionTLS *connTls =
          new ConnectionTLS(j, // ConnectionDataHandler (Client implements this)
                            conn0,           // Underlying TCP connection
                            j->logInstance() // LogSink
          );
      // Set server name for TLS SNI (Server Name Indication)
      connTls->setServer(BoshHost);
      connBase = connTls;

      if (DebugLog || true) {
        cout << "  TLS layer added for HTTPS, SNI host: " << BoshHost << endl;
      }
    } else {
      // HTTP mode - no TLS layer (external proxy handles TLS)
      if (DebugLog || true) {
        cout << "  HTTP mode - no TLS layer (external proxy handles TLS)"
             << endl;
      }
    }

    // Wrap with BOSH layer (use domain name for Host header)
    // Constructor: ConnectionBOSH(ConnectionDataHandler*, ConnectionBase*,
    // LogSink&, boshHost, xmppServer, xmppPort)
    ConnectionBOSH *conn1 = new ConnectionBOSH(
        j,        // Client (implements ConnectionDataHandler)
        connBase, // TCP connection (raw for HTTP, TLS-wrapped for HTTPS)
        j->logInstance(), // LogSink
        BoshHost,         // BOSH hostname (for HTTP Host header - domain name)
        BoshHost,         // XMPP server name (domain name)
        urlParts.port     // Use actual connection port
    );

    // Set the BOSH path (from the parsed URL) - CRITICAL for HTTP 400 fix
    conn1->setPath(urlParts.path);

    if (DebugLog || true) {
      cout << "  BOSH path set to: " << urlParts.path << endl;
    }

    // Use HTTP Pipelining mode (single connection, avoids TLS pool issues)
    conn1->setMode(ConnectionBOSH::ModePipelining);

    if (DebugLog || true) {
      cout << "  BOSH mode: HTTP Pipelining (single connection)" << endl;
    }

    // Attach custom connection to client
    j->setConnectionImpl(conn1);

    // TLS settings for BOSH
    if (!tlsEnabled) {
      // XMPP TLS disabled - for HTTP BOSH with external TLS proxy
      j->setTls(TLSPolicy::TLSDisabled);
      if (DebugLog || true)
        cout << "  TLS: Disabled (xmpptls=false, external TLS proxy)" << endl;
    } else if (TlsVerify) {
      j->setTls(TLSPolicy::TLSRequired);
      if (DebugLog)
        cout << "  TLS: Required with certificate validation" << endl;
    } else {
      j->setTls(TLSPolicy::TLSOptional);
      if (DebugLog || true)
        cout << "  TLS: Optional (certificate validation DISABLED)" << endl;
    }

    if (DebugLog || true)
      cout << "========================================================="
           << endl;

  } else {
    // Traditional TCP mode (existing behavior)
    j->setServer(server);
    j->setPort(5222);
    if (DebugLog)
      cout << "ADXmppProxy::connect: TCP mode on port 5222, server: " << server
           << endl;
  }

  // Common setup for both modes
  j->registerConnectionListener(this);
  j->registerMessageSessionHandler(this, 0);
  j->rosterManager()->registerRosterListener(this);
  j->disco()->setVersion("messageTest", GLOOX_VERSION, "Linux");
  j->disco()->setIdentity("client", "jsonbot");
  j->disco()->addFeature(XMLNS_CHAT_STATES);

  // LogLevelDebug
  if (DebugLog)
    j->logInstance().registerLogHandler(LogLevelDebug, LogAreaAll, this);
  else
    j->logInstance().registerLogHandler(LogLevelWarning, LogAreaAll, this);

  HeartBeat = 0;

  ConnectionError ce = ConnNoError;
  if (j->connect(false)) {
    while (ce == ConnNoError) {
      if (UseBOSH) {
        // blocking recv(): timed recv exceeds the BOSH request limit
        ce = j->recv();
      } else {
        // timed recv(): between polls this thread drains the outbound
        // queue, so every gloox call stays on this thread
        ce = j->recv(ADXMPP_RECV_POLL_US);
      }
      if (DisconnectRequested) {
        XMLOG_INF("xmpp: disconnect requested, closing session");
        j->disconnect();
        if (ce == ConnNoError)
          ce = ConnUserDisconnected;
        break;
      }
      if (!UseBOSH)
        flush_outbound();
    }

    // Enhanced error logging (especially for BOSH)
    if (ce != ConnNoError) {
      if (DebugLog || UseBOSH) { // Always log errors in BOSH mode
        cout << "ADXmppProxy::connect: Connection ended with error" << endl;
        cout << "  Error code: " << ce << endl;
        cout << "  Mode: " << (UseBOSH ? "BOSH" : "TCP") << endl;

        // Connection error descriptions
        switch (ce) {
        case ConnAuthenticationFailed:
          cout << "  Reason: Authentication failed (bad credentials?)" << endl;
          break;
        case ConnDnsError:
          cout << "  Reason: DNS error (cannot resolve hostname)" << endl;
          break;
        case ConnConnectionRefused:
          cout << "  Reason: Connection refused (firewall/port blocked?)"
               << endl;
          break;
        case ConnTlsFailed:
          cout << "  Reason: TLS handshake failed (certificate issue?)" << endl;
          break;
        case ConnStreamError:
          cout << "  Reason: XMPP stream error" << endl;
          break;
        case ConnNotConnected:
          cout << "  Reason: Not connected" << endl;
          break;
        default:
          cout << "  Reason: Unknown error" << endl;
          break;
        }

        if (UseBOSH) {
          cout << "  Troubleshooting:" << endl;
          cout << "    - Verify BOSH URL is correct: " << BoshURL << endl;
          cout << "    - Check server supports BOSH at /http-bind" << endl;
          cout << "    - Test with: curl -v " << BoshURL << endl;
          if (!TlsVerify) {
            cout << "    - TLS verification is DISABLED (cert mismatch allowed)"
                 << endl;
          }
        }
      }
    } else {
      if (DebugLog)
        cout << "ADXmppProxy::connect: Connection closed normally" << endl;
    }
  } else {
    // Connection attempt failed immediately
    if (DebugLog || UseBOSH) {
      cout << "ADXmppProxy::connect: Failed to initiate connection" << endl;
      cout << "  Mode: " << (UseBOSH ? "BOSH" : "TCP") << endl;
      if (UseBOSH) {
        cout << "  BOSH URL: " << BoshURL << endl;
        cout << "  Check network connectivity and firewall settings" << endl;
      }
    }
  }

  usleep(100000);

  {
    std::lock_guard<std::mutex> lock(clientMutex);
    // sessions belong to the client being destroyed
    mySessions.clear();
    delete (j);
    j = NULL;
  }
  {
    std::lock_guard<std::mutex> lock(outMutex);
    outQueue.clear(); // nothing left to deliver them to
  }
  {
    std::lock_guard<std::mutex> lock(rosterMutex);
    rosterJids.clear();
    rosterOnline.clear();
  }
  iConnect = false;
  connected = false;
  SessionRunning = false;

  if (DebugLog)
    cout << "ADXmppProxy::connect: exiting<===" << endl;

  return 0;
}
/*****************************************************************************/
void ADXmppProxy::SetDebugLog(bool log) { DebugLog = log; }
/*****************************************************************************/
void ADXmppProxy::onConnect() {
  XMLOG_INF("xmpp: session established (%s)", UseBOSH ? "BOSH" : "TCP");
  iConnect = true;
  SessionAuthenticated = true;
  HeartBeat = 0;
}
/*****************************************************************************/
void ADXmppProxy::onDisconnect(ConnectionError e) {
  if (e == ConnAuthenticationFailed) {
    failed_authorization = true;
    if (DebugLog)
      cout
          << "ADXmppProxy::onDisconnect:disconnected due to failed authrization"
          << endl;
  }
  XMLOG_INF("xmpp: session ended (error code %d)", (int)e);
  iConnect = false;
}
/*****************************************************************************/
bool ADXmppProxy::onTLSConnect(const CertInfo &info) {
  if (DebugLog) {
    cout << "ADXmppProxy::onTLSConnect:called!!!" << endl;
    time_t from(info.date_from);
    time_t to(info.date_to);
    printf("status: %d\nissuer: %s\npeer: %s\nprotocol: %s\nmac: %s\ncipher: "
           "%s\ncompression: %s\n",
           info.status, info.issuer.c_str(), info.server.c_str(),
           info.protocol.c_str(), info.mac.c_str(), info.cipher.c_str(),
           info.compression.c_str());
    printf("from: %s", ctime(&from));
    printf("to:   %s", ctime(&to));
  }
  return true;
}
/*****************************************************************************/
void ADXmppProxy::handleMessage(const Message &msg, MessageSession *session) {
  if (DebugLog)
    cout << "ADXmppProxy::handleMessage:arrived:msg:" << msg.body()
         << ":len:" << msg.body().size() << " from=" << session->target().bare()
         << endl;
  if (msg.body().size() > 0)
    receive_request(msg.body(), session->target().bare());
  // else
  // ignore-the-message
}
/*****************************************************************************/
int ADXmppProxy::receive_request(std::string req, std::string sender) {
  if (DebugLog)
    cout << "ADXmppProxy::receive_request:received from=" << sender
         << " msg=" << req << endl;

  // if sender is admin, then just process the request else check if sender is
  // in our BuddyList.
  if (is_admin_user(sender)) {
    onXmppMessage(req, sender);
    return 0;
  }

  // process sender message only if sender is part of our roster(respond to only
  // authorized users)
  if (IsMyBuddy(sender)) {
    onXmppMessage(req,
                  sender); // callback to the attached msg-processing-object
    return 0;
  }

  // process sender message only if sender is part of our roster(respond to only
  // authorized users) vector<std::string>::iterator it; for(it =
  // BuddyList.begin(); it != BuddyList.end(); it++)
  //{
  //	string str = *it;
  //	if(sender==str)
  //	{
  //		onXmppMessage(req,sender);//callback to the attached
  // msg-processing-object 		return 0;
  //	}
  // }
  if (DebugLog)
    cout << "ADXmppProxy::receive_request:sender is not authorized!!! "
            "ingnoring the request"
         << endl;
  return -1;
}
int ADXmppProxy::send_reply(std::string reply, std::string sender) {
  if (sender.empty())
    return -1;
  return enqueue(OutItem(OutItem::MSG, sender, reply, gloox::EmptyString));

  // send response
  // m_messageEventFilter->raiseMessageEvent( MessageEventDisplayed );
  // m_messageEventFilter->raiseMessageEvent( MessageEventComposing );
  // m_chatStateFilter->setChatState( ChatStateComposing );
  // m_session->send( reply, gloox::EmptyString );//after reply from
  // json-rpc-server, call send reply if(DebugLog)
  //	cout<<"ADXmppProxy::send_reply:to:"<<sender<<" sending msg =
  //"<<reply<<endl; return 0;
}
/*****************************************************************************/
void ADXmppProxy::handleMessageEvent(const JID &from, MessageEventType event) {
  if (DebugLog)
    printf("received event: %d from: %s\n", event, from.full().c_str());
}
/*****************************************************************************/
void ADXmppProxy::handleChatState(const JID &from, ChatStateType state) {
  if (DebugLog)
    printf("received state: %d from: %s\n", state, from.full().c_str());
}
/*****************************************************************************/
// this function is called first time when client starts chat(after this,
// handleMessage is called)
void ADXmppProxy::handleMessageSession(MessageSession *session) {
  if (j == NULL)
    return;
  const gloox::JID &jid = session->target();
  if (DebugLog)
    cout << "Got new message session for " << jid.full().c_str() << endl;
  Sessions::iterator it = mySessions.find(jid.bare());
  if (it != mySessions.end()) {
    if (DebugLog)
      cout << "Disposing existing message session for "
           << it->second.m_session->target().full().c_str() << endl;
    j->disposeMessageSession(it->second.m_session);
    mySessions.erase(it);
  }
  session->registerMessageHandler(this);
  gloox::ChatStateFilter *filter = new gloox::ChatStateFilter(session);
  gloox::MessageEventFilter *evntfilter =
      new gloox::MessageEventFilter(session);
  filter->registerChatStateHandler(this);
  evntfilter->registerMessageEventHandler(this);
  Session newSession;
  newSession.m_session = session;
  newSession.m_chatStateFilter = filter;
  newSession.m_messageEventFilter = evntfilter;
  mySessions[jid.bare()] = newSession;

  // printf( "got new session\n");
  //  this example can handle only one session. so we get rid of the old session
  /*if(m_session)
          j->disposeMessageSession( m_session );
  m_session = session;
  m_session->registerMessageHandler( this );
  m_messageEventFilter = new MessageEventFilter( m_session );
  m_messageEventFilter->registerMessageEventHandler( this );
  m_chatStateFilter = new ChatStateFilter( m_session );
  m_chatStateFilter->registerChatStateHandler( this );*/
}
/*****************************************************************************/
void ADXmppProxy::handleLog(LogLevel level, LogArea area,
                            const std::string &message) {
  if (DebugLog)
    printf("log: level: %d, area: %d, %s\n", level, area, message.c_str());
}
/*****************************************************************************/
// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
const std::string ADXmppProxy::currentDateTime() {
  time_t now = time(0);
  struct tm tstruct;
  char buf[80];
  tstruct = *localtime(&now);
  strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
  return buf;
}
void ADXmppProxy::send_client_alive_ping() {
  // called from the timer thread; the ping itself is sent by the session
  // thread (TCP) or under clientMutex (BOSH)
  enqueue(OutItem(OutItem::PING));
}
/*****************************************************************************/
int ADXmppProxy::enqueue(const OutItem &item) {
  if (!connected)
    return -1; // no session: nothing could deliver it
  {
    std::lock_guard<std::mutex> lock(outMutex);
    if (outQueue.size() >= ADXMPP_OUTBOUND_QUEUE_MAX) {
      XMLOG_WRN("xmpp: outbound queue full, dropping item for %s",
                item.to.c_str());
      return -1;
    }
    outQueue.push_back(item);
  }
  if (UseBOSH) {
    // recv() blocks in BOSH mode, so the session thread never gets to
    // flush: send from here under the client lock (documented limitation)
    std::lock_guard<std::mutex> lock(clientMutex);
    if (j != NULL)
      flush_outbound();
  }
  return 0;
}
/*****************************************************************************/
void ADXmppProxy::flush_outbound() {
  std::deque<OutItem> items;
  {
    std::lock_guard<std::mutex> lock(outMutex);
    items.swap(outQueue);
  }
  if (j == NULL)
    return;
  for (std::deque<OutItem>::iterator it = items.begin(); it != items.end();
       ++it)
    perform(*it);
}
/*****************************************************************************/
void ADXmppProxy::perform(const OutItem &item) {
  switch (item.type) {
  case OutItem::MSG: {
    Sessions::iterator it = mySessions.find(item.to);
    if (it != mySessions.end()) {
      it->second.m_messageEventFilter->raiseMessageEvent(MessageEventDisplayed);
      it->second.m_messageEventFilter->raiseMessageEvent(MessageEventComposing);
      it->second.m_chatStateFilter->setChatState(ChatStateComposing);
      it->second.m_session->send(item.body, item.subject);
    } else {
      // no chat session with this buddy yet: one-off session
      MessageSession *session = new MessageSession(j, JID(item.to));
      session->send(item.body, item.subject);
      j->disposeMessageSession(session);
    }
    XMLOG_DBG("xmpp: sent to %s: %s", item.to.c_str(), item.body.c_str());
  } break;
  case OutItem::PING:
    j->xmppPing(j->jid(), this);
    if (++HeartBeat > MaxMissedPongs) {
      XMLOG_WRN("xmpp: %d keepalive pings unanswered, reconnecting",
                HeartBeat - 1);
      HeartBeat = 0;
      j->disconnect();
    } else {
      XMLOG_DBG("xmpp: keepalive ping sent (%d outstanding)", HeartBeat);
    }
    break;
  case OutItem::SUBSCRIBE:
    j->rosterManager()->subscribe(JID(item.to));
    break;
  case OutItem::UNSUBSCRIBE:
    j->rosterManager()->unsubscribe(JID(item.to));
    break;
  }
}
/*****************************************************************************/
void ADXmppProxy::mirror_roster_from(const Roster &roster) {
  std::lock_guard<std::mutex> lock(rosterMutex);
  rosterJids.clear();
  for (Roster::const_iterator it = roster.begin(); it != roster.end(); ++it)
    rosterJids.insert((*it).second->jidJID().bare());
}
/*****************************************************************************/
void ADXmppProxy::handleEvent(const Event &event) {
  switch (event.eventType()) {
  case Event::PingPing:
    break;
  case Event::PingPong:
    --HeartBeat;
    break;
  case Event::PingError:
    break;
  default:
    break;
  }
  // cout<<"handleEvent::pong received"<<endl;
}
/*****************************************************************************/
void ADXmppProxy::onResourceBindError(ResourceBindError error) {
  // printf( "onResourceBindError: %d\n", error );
  if (DebugLog)
    cout << "ADXmppProxy::onResourceBindError: error:" << error << endl;
}
void ADXmppProxy::onSessionCreateError(SessionCreateError error) {
  // printf( "onSessionCreateError: %d\n", error );
  if (DebugLog)
    cout << "ADXmppProxy::onSessionCreateError: error:" << error << endl;
}
void ADXmppProxy::handleItemSubscribed(const JID &jid) {
  XMLOG_INF("xmpp: roster subscription confirmed for %s", jid.bare().c_str());
  std::lock_guard<std::mutex> lock(rosterMutex);
  rosterJids.insert(jid.bare());
}
void ADXmppProxy::handleItemAdded(const JID &jid) {
  XMLOG_INF("xmpp: roster item added: %s", jid.bare().c_str());
  {
    std::lock_guard<std::mutex> lock(rosterMutex);
    rosterJids.insert(jid.bare());
  }

  // Roster *pRoster=j->rosterManager()->roster();
  // Roster::const_iterator it = pRoster->begin();
  // for( ; it != pRoster->end(); ++it )
  //{
  //	cout<<"ADXmppProxy::handleItemAdded:"<<(*it).second->jidJID().full()<<endl;
  // }
}
void ADXmppProxy::handleItemUnsubscribed(const JID &jid) {
  // printf( "unsubscribed %s\n", jid.bare().c_str() );
  if (DebugLog)
    cout << "ADXmppProxy::handleItemUnsubscribed: unsubscribed:"
         << jid.bare().c_str() << endl;
}
void ADXmppProxy::handleItemRemoved(const JID &jid) {
  XMLOG_INF("xmpp: roster item removed: %s", jid.bare().c_str());
  std::lock_guard<std::mutex> lock(rosterMutex);
  rosterJids.erase(jid.bare());
  rosterOnline.erase(jid.bare());
}
void ADXmppProxy::handleItemUpdated(const JID &jid) {
  // printf( "updated %s\n", jid.bare().c_str() );
  if (DebugLog)
    cout << "ADXmppProxy::handleItemUpdated: updated:" << jid.bare().c_str()
         << endl;
}
void ADXmppProxy::handleRoster(const Roster &roster) {
  mirror_roster_from(roster);
  XMLOG_INF("xmpp: roster received with %d item(s)", (int)roster.size());
  Roster::const_iterator it = roster.begin();
  for (; it != roster.end(); ++it) {
    /*printf( "jid: %s, name: %s, subscription: %d\n",
    (*it).second->jidJID().full().c_str(), (*it).second->name().c_str(),
    (*it).second->subscription() );
    StringList g = (*it).second->groups();
    StringList::const_iterator it_g = g.begin();
    for( ; it_g != g.end(); ++it_g )
            printf( "\tgroup: %s\n", (*it_g).c_str() );
    RosterItem::ResourceMap::const_iterator rit =
    (*it).second->resources().begin(); for( ; rit !=
    (*it).second->resources().end(); ++rit ) printf( "resource: %s\n",
    (*rit).first.c_str() );*/

    // vector<std::string>::iterator bit;
    // for(bit = BuddyList.begin(); bit != BuddyList.end(); bit++)
    //{
    //	string str = *bit;
    //	if((*it).second->jidJID().full()==str) //if this buddy is already in our
    // list, then dont add duplicate entry 		continue;
    // }
    // BuddyList.push_back((*it).second->jidJID().full());//this user is not
    // found in our BuddyList, hence add to our list
    if (DebugLog)
      cout << "ADXmppProxy::handleRoster:" << endl;
  }
}
void ADXmppProxy::handleRosterError(const IQ & /*iq*/) {
  // printf( "a roster-related error occured\n" );
  if (DebugLog)
    cout << "ADXmppProxy::handleRosterError: roster-related error occured"
         << endl;
}
void ADXmppProxy::handleRosterPresence(const RosterItem &item,
                                       const std::string &resource,
                                       Presence::PresenceType presence,
                                       const std::string & /*msg*/) {
  // printf( "presence received: %s/%s -- %d\n", item.jidJID().full().c_str(),
  // resource.c_str(), presence );
  if (DebugLog)
    cout << "ADXmppProxy::handleRosterPresence: received:"
         << item.jidJID().full().c_str() << ":" << resource.c_str() << ":"
         << convert_presence_enum_to_str(presence) << endl;
  std::lock_guard<std::mutex> lock(rosterMutex);
  rosterOnline[item.jidJID().bare()] = item.online();
}
void ADXmppProxy::handleSelfPresence(const RosterItem &item,
                                     const std::string &resource,
                                     Presence::PresenceType presence,
                                     const std::string & /*msg*/) {
  // printf( "self presence received: %s/%s -- %d\n",
  // item.jidJID().full().c_str(), resource.c_str(), presence );
  if (DebugLog)
    cout << "ADXmppProxy::handleSelfPresence: received:"
         << item.jidJID().full().c_str() << ":" << resource.c_str() << ":"
         << presence << endl;
}
bool ADXmppProxy::handleSubscriptionRequest(const JID &jid,
                                            const std::string & /*msg*/) {
  if (DebugLog)
    cout << "ADXmppProxy::handleSubscriptionRequest:from:" << jid.bare().c_str()
         << endl;

  // check if subscriber-buddy needs to be accepted based on available
  // cmdline-arg or through some other means(AdminBuddy), if subscriber-buddy is
  // to be accepted, then accept the subscription and also send
  // subscription-request to buddy in return
  if (is_admin_user(jid.bare())) {
    // StringList groups;
    // JID id( jid );
    // j->rosterManager()->subscribe( id, "", groups, "" );
    if (DebugLog)
      cout << "ADXmppProxy::handleSubscriptionRequest:AdminBuddy has been "
              "accepted:"
           << AdminBuddy << endl;
    return true; // true;
  } else {
    // check if this buddy is to be accepted if available in
    // approved-AcceptBuddyList
    std::string tmp = jid.bare().c_str();
    XMLOG_INF("xmpp: subscription request from %s", tmp.c_str());
    std::lock_guard<std::mutex> lock(acceptMutex);
    std::vector<string>::iterator itr = std::find(
        AcceptBuddyList.begin(), AcceptBuddyList.end(), tmp); // jid.bare());
    if (itr != AcceptBuddyList.end()) {
      // StringList groups;
      // JID id( jid );
      // j->rosterManager()->subscribe( id, "", groups, "" );
      if (DebugLog)
        cout << "ADXmppProxy::handleSubscriptionRequest:Accepting pre-approved "
                "buddy:"
             << *itr << endl;
      return true;
    }

    /*vector<std::string>::iterator bit;
    for(bit = AcceptBuddyList.begin(); bit != AcceptBuddyList.end(); bit++)
    {
            string str = *bit;
            if(jid.bare() == str) //accept this buddy as this buddy is already
    part of our approved list
            {
                    StringList groups;
                    JID id( jid );
                    j->rosterManager()->subscribe( id, "", groups, "" );
                    if(DebugLog)
                            cout<<"ADXmppProxy::handleSubscriptionRequest:Accepting
    pre-approved buddy:"<<str<<endl; return true;
            }
    }*/
    if (DebugLog)
      cout << "ADXmppProxy::handleSubscriptionRequest: this buddy has not been "
              "authorized: "
           << jid << endl;
    return false;
  }
}
bool ADXmppProxy::handleUnsubscriptionRequest(const JID &jid,
                                              const std::string & /*msg*/) {
  // printf( "unsubscription: %s\n", jid.bare().c_str() );
  // TODO: remove this user from our BuddyList so that we dont send events to
  // this user.
  if (DebugLog)
    cout << "ADXmppProxy::handleUnsubscriptionRequest: user "
         << jid.bare().c_str() << " removed from buddy-list" << endl;
  remove_buddy(jid.bare().c_str()); // remove from our buddy list : TODO: check
                                    // if this entry is also removed from roster
  // unsubscribe_buddy(std::string buddy); //TODO: check if mutual
  // unsubscription is needed
  return true;
}
void ADXmppProxy::handleNonrosterPresence(const Presence &presence) {
  // printf( "received presence from entity not in the roster: %s\n",
  // presence.from().full().c_str() );
  if (DebugLog)
    cout << "ADXmppProxy::handleNonrosterPresence: entity not in the roster: "
         << presence.from().full().c_str() << endl;
}
/* ------------------------------------------------------------------------- */
// following function is used for sending async-event-notification to subscribed
// buddy
bool ADXmppProxy::SendMessageToBuddy(std::string address,
                                     const std::string &body,
                                     const std::string &subject) {
  if (iConnect == false) {
    XMLOG_DBG("xmpp: not connected, cannot send to %s", address.c_str());
    return false;
  }
  // only roster members: a message to a non-buddy would not be delivered
  if (!IsMyBuddy(address))
    return false;
  return enqueue(OutItem(OutItem::MSG, address, body, subject)) == 0;
}
/* ------------------------------------------------------------------------- */
bool ADXmppProxy::IsMyBuddy(std::string buddyaddress) {
  std::lock_guard<std::mutex> lock(rosterMutex);
  return rosterJids.find(buddyaddress) != rosterJids.end();
}
/* ------------------------------------------------------------------------- */
int ADXmppProxy::get_buddy_list(std::string &returnval) {
  std::lock_guard<std::mutex> lock(rosterMutex);
  for (std::set<std::string>::iterator it = rosterJids.begin();
       it != rosterJids.end(); ++it) {
    returnval += *it;
    returnval += '\n';
  }
  return 0;
}
/* ------------------------------------------------------------------------- */
bool ADXmppProxy::get_connected_status() { return iConnect; }
/* ------------------------------------------------------------------------- */
std::string
ADXmppProxy::convert_presence_enum_to_str(Presence::PresenceType presence) {
  switch (presence) {
  case Presence::Available:
    return "Available";
  case Presence::Chat:
    return "Chat";
  case Presence::Away:
    return "Away";
  case Presence::DND: // do-not-disturb
    return "DND";
  case Presence::XA: // extended-away
    return "XA";
  case Presence::Unavailable:
    return "Unavailable";
  case Presence::Probe:
    return "Probe";
  case Presence::Error:
    return "Error";
  case Presence::Invalid:
    return "Invalid";
  default:
    return "Unknown";
  }
}
/* ------------------------------------------------------------------------- */
// note: only admin-buddy shall call this function
int ADXmppProxy::accept_buddy(std::string buddy) {
  std::lock_guard<std::mutex> lock(acceptMutex);
  vector<std::string>::iterator bit;
  for (bit = AcceptBuddyList.begin(); bit != AcceptBuddyList.end(); bit++) {
    string str = *bit;
    if (buddy ==
        str) // if this buddy is already in our list, then just return success
    {
      if (DebugLog)
        cout << "ADXmppProxy::accept_buddy:buddy is already in our list:"
             << buddy << endl;
      return 0;
    }
  }
  AcceptBuddyList.push_back(
      buddy); // next-time, when this buddy subscribes, then accept it
  if (DebugLog)
    cout << "ADXmppProxy::accept_buddy:buddy has been added to our list:"
         << buddy << endl;
  return 0;
}
int ADXmppProxy::remove_buddy(std::string buddy) {
  std::lock_guard<std::mutex> lock(acceptMutex);
  std::vector<string>::iterator itr =
      std::find(AcceptBuddyList.begin(), AcceptBuddyList.end(), buddy);
  if (itr != AcceptBuddyList.end()) {
    AcceptBuddyList.erase(itr);
    if (DebugLog)
      cout << "ADXmppProxy::remove_buddy:buddy has been removed from our list:"
           << buddy << endl;
    return 0;
  } else {
    if (DebugLog)
      cout
          << "ADXmppProxy::remove_buddy:requested buddy is not in the list!!!!:"
          << buddy << endl;
    return -1;
  }
}
/* ------------------------------------------------------------------------- */
bool ADXmppProxy::is_admin_user(std::string user) {
  std::lock_guard<std::mutex> lock(rosterMutex);
  if (user == AdminBuddy || user == BkupAdminBuddy)
    return true;
  else
    return false;
}
/* ------------------------------------------------------------------------- */
int ADXmppProxy::subscribe_buddy(std::string buddy) {
  return enqueue(OutItem(OutItem::SUBSCRIBE, buddy));
}
/* ------------------------------------------------------------------------- */
int ADXmppProxy::unsubscribe_buddy(std::string buddy) {
  return enqueue(OutItem(OutItem::UNSUBSCRIBE, buddy));
}
/* ------------------------------------------------------------------------- */
int ADXmppProxy::get_buddy_online_state(std::string buddy) {
  std::lock_guard<std::mutex> lock(rosterMutex);
  std::map<std::string, bool>::iterator it = rosterOnline.find(buddy);
  return (it != rosterOnline.end() && it->second) ? 1 : 0;
}
/* ------------------------------------------------------------------------- */
int ADXmppProxy::get_accept_buddy_list(std::string &returnval) {
  std::lock_guard<std::mutex> lock(acceptMutex);
  vector<std::string>::iterator it;
  for (it = AcceptBuddyList.begin(); it != AcceptBuddyList.end(); it++) {
    returnval += *it;
    returnval += '\n';
  }
  // Roster *pRoster=j->rosterManager()->roster();
  // Roster::const_iterator it = pRoster->begin();
  // for( ; it != pRoster->end(); ++it )
  //{
  //	returnval+=(*it).second->jidJID().full();
  //	returnval+='\n';
  // }
  return 0;
}
/* ------------------------------------------------------------------------- */
