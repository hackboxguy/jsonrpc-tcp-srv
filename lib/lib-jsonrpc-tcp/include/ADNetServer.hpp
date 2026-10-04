#ifndef __ADNETSERVER_H_
#define __ADNETSERVER_H_
#include "ADCommon.hpp"
#include "ADGenericChain.hpp"
#include "ADJsonStreamFramer.hpp"
#include "ADThread.hpp"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define AD_NET_SERVER_DEFAULT_LISTEN_PORT 65001
#define AD_NET_SERVER_MAX_BUFFER_SIZE 0xffff
// upper bound for one JSON request; larger requests close the connection
#define AD_NET_SERVER_MAX_JSON_MSG_SIZE (256 * 1024)
// how long the response thread waits for a slow reader per response; a
// connection whose response cannot be sent in time is dropped (V2-C3)
#define AD_NET_SERVER_SEND_TIMEOUT_MS 1000
// a connection whose responses made the response thread wait more than this
// in total within AD_NET_SERVER_SEND_WAIT_WINDOW_MS is dropped as well: a
// slow reader that never stalls a full timeout must not delay all others
// for long (V3-M4)
#define AD_NET_SERVER_SEND_WAIT_BUDGET_MS 5000
#define AD_NET_SERVER_SEND_WAIT_WINDOW_MS 60000
// requests of one connection that may wait for their response; above it the
// server stops reading from that connection until half are answered (V2-C3)
#define AD_NET_SERVER_MAX_PENDING_PER_CONN 256
// TCP keepalive on accepted connections: a peer that vanished without FIN
// is detected after about IDLE + INTVL * CNT seconds (V2-M1)
#define AD_NET_SERVER_KEEPALIVE_IDLE_S 60
#define AD_NET_SERVER_KEEPALIVE_INTVL_S 10
#define AD_NET_SERVER_KEEPALIVE_CNT 5
typedef enum ADLIB_TCP_SOCKET_TYPE_T {
  ADLIB_TCP_SOCKET_TYPE_BINARY,
  ADLIB_TCP_SOCKET_TYPE_JSON,
  ADLIB_TCP_SOCKET_TYPE_UNKNOWN,
  ADLIB_TCP_SOCKET_TYPE_NONE
} ADLIB_TCP_SOCKET_TYPE;
class ADNetProducer;
class ADNetConsumer {
public:
  virtual int on_data_arrival(ADGenericChain *pRxChain,
                              ADNetProducer *pObj) = 0;
  virtual ~ADNetConsumer(){};
};
class ADNetProducer {
  static int IDGenerator;
  ADNetConsumer *pConsumer;
  int id;

protected:
  int notify_data_arrival(ADGenericChain *pRxChain) {
    if (pConsumer != NULL)
      return pConsumer->on_data_arrival(pRxChain, this);
    return -1;
  }
  int is_helper_attached(void) {
    if (pConsumer == NULL)
      return -1;
    return 0;
  }

public:
  ADNetProducer() {
    id = IDGenerator++;
    pConsumer = NULL;
  }
  virtual ~ADNetProducer(){};
  int attach_on_data_arrival(ADNetConsumer *c) {
    if (pConsumer == NULL) {
      pConsumer = c;
      return id;
    } else
      return -1;
  }
  int getID() { return id; }
};
struct net_data_obj {
  int ident;
  int sock_descriptor;
  int port;
  char ip[512];
  int cltid;
  int data_buffer_len;
  char *data_buffer;

public:
  // cltid -1 means 'connection unknown' (responses skip the fd-reuse check)
  net_data_obj()
      : ident(0), sock_descriptor(-1), port(-1), cltid(-1), data_buffer_len(0),
        data_buffer(NULL) {
    ip[0] = '\0';
  };
  ~net_data_obj(){};
};
class ADNetServer : public ADNetProducer,
                    public ADChainConsumer,
                    public ADThreadConsumer {
  ADLIB_TCP_SOCKET_TYPE sock_type;
  int socketlog;
  int connected;
  unsigned char end_server;
  int listen_port;
  int listen_sd;
  in_addr_t bind_address; // network byte order, INADDR_ANY by default
  int wake_pipe[2]; // written by stop_receiving() to end select() at once
  int max_sd;
  struct sockaddr_in addr;
  struct timeval timeout;
  fd_set master_set;
  char receive_buffer[AD_NET_SERVER_MAX_BUFFER_SIZE];
  int receive_size;
  // one stream framer per JSON connection; only used by the listen thread
  std::map<int, ADJsonStreamFramer *> framers;
  // outstanding requests per connection (by cltid), guarded by pending_lock
  struct conn_pending {
    int count;
    bool paused;
    // dropped by the response thread (V3-H2): no more requests are queued and
    // no responses sent; the listen thread still owns and closes the fd
    bool dead;
    long wait_ms;         // send waits in the current window
    long window_start_ms; // monotonic start of that window
    conn_pending()
        : count(0), paused(false), dead(false), wait_ms(0), window_start_ms(0) {
    }
  };
  pthread_mutex_t pending_lock;
  pthread_mutex_t ctrl_lock; // serializes start/stop
  int stop_receiving_locked();
  std::map<int, conn_pending> pending;
  std::set<int> paused_fds; // listen thread only
  int queue_framed_requests(int socket_descriptor);
  void response_done(int cltid);
  void resume_paused_connections();
  void drop_connection(int dup_fd, int socket_descriptor, int cltid,
                       bool timed_out);
  bool connection_dead(int cltid);
  int id_listen_thread;
  int id_response_thread;
  ADThread listen_thread;
  ADThread response_thread;
  ADGenericChain request_chain;
  ADGenericChain response_chain;
  ADGenericChain clientInfo_chain;
  virtual int identify_chain_element(void *element, int ident,
                                     ADChainProducer *pObj);
  virtual int double_identify_chain_element(void *element, int ident1,
                                            int ident2, ADChainProducer *pObj);
  virtual int free_chain_element_data(void *element, ADChainProducer *pObj);
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj);
  virtual int thread_callback_function(void *pUserData, ADThreadProducer *pObj);
  int initialize_helpers(void);
  int print_client_info(struct sockaddr *in_addr, socklen_t in_len,
                        int sock_descr);
  int register_client_info(int sock_descr, int port, char *ip);
  int deregister_client_info(int sock_descr);
  int get_client_info(int sock_descr, char *cltip, int *cltport, int *cltid);
  int binary_receive_data_and_notify_consumer(int socket_descriptor, char *buf,
                                              int len);
  int json_receive_data_and_notify_consumer(int socket_descriptor, char *buf,
                                            int len);
  void send_protocol_error(int socket_descriptor);
  void close_connection(int socket_descriptor);
  int dup_if_same_client(int socket_descriptor, int cltid);
  int send_with_deadline(int socket_descriptor, const char *buf, int len,
                         long *waited_ms);
  bool over_send_wait_budget(int cltid, long waited_ms);
  int start_listening();
  int stop_listening();
  bool IsConnectionAlive(int sock_descriptor);

public:
  ADNetServer();
  ADNetServer(int port);
  ~ADNetServer();
  // stops accepting and reading (listen thread); queued responses are
  // still sent until stop() is called
  int stop_receiving();
  // stops both threads and closes all connections; idempotent
  int stop();
  int schedule_response(int socket_descriptor, char *buf, int len);
  // cltid identifies the connection (net_data_obj::cltid of the request);
  // the response is dropped if that connection was closed meanwhile, even
  // when the fd number was already reused by a new client. -1 skips the check
  int schedule_response(int socket_descriptor, int cltid, char *buf, int len);
  // returns 0, or -1 with errno set (the reason is also logged)
  int start_listening(
      int port, int socket_log,
      ADLIB_TCP_SOCKET_TYPE socket_type = ADLIB_TCP_SOCKET_TYPE_JSON);
  // IPv4 address to listen on (e.g. "127.0.0.1"); call before
  // start_listening(). Default: all interfaces.
  int set_bind_address(const char *ip);
};
#endif
