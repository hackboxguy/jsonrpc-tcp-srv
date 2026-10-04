#include "ADNetServer.hpp"
#include "ADCommon.hpp"
#include <iostream>
#include <poll.h>
using namespace std;
#define AD_NETWORK_TRUE 1
#define AD_NETWORK_FALSE 0
int ADNetProducer::IDGenerator = 0;
static bool would_block(int err) {
#if EAGAIN != EWOULDBLOCK
  if (err == EWOULDBLOCK)
    return true;
#endif
  return err == EAGAIN;
}
int ADNetServer::identify_chain_element(void *element, int ident,
                                        ADChainProducer *pObj) {
  net_data_obj *pPtr;
  pPtr = (net_data_obj *)element;
  if (pPtr->sock_descriptor == ident)
    return 0;
  else
    return -1;
}
int ADNetServer::double_identify_chain_element(void *element, int ident1,
                                               int ident2,
                                               ADChainProducer *pObj) {
  net_data_obj *pPtr;
  pPtr = (net_data_obj *)element;
  if (pPtr->sock_descriptor == ident1 && pPtr->ident == ident2)
    return 0;
  else
    return -1;
}
int ADNetServer::free_chain_element_data(void *element, ADChainProducer *pObj) {
  net_data_obj *obj;
  obj = (net_data_obj *)element;
  if (obj->data_buffer != NULL)
    ARRAY_MEM_DELETE(obj->data_buffer);
  return 0;
}
// Returns a private close-on-exec dup of the client socket if sock_descriptor
// still belongs to the connection identified by cltid, else -1. The check and
// the dup() happen under the client-info lock, and the listen thread
// deregisters a connection before it closes the fd, so the dup always refers to
// the original connection even if the fd number gets reused afterwards.
int ADNetServer::dup_if_same_client(int sock_descriptor, int cltid) {
  int fd = -1;
  if (cltid < 0) {
    if (IsConnectionAlive(sock_descriptor))
      fd = fcntl(sock_descriptor, F_DUPFD_CLOEXEC, 0);
    return fd;
  }
  clientInfo_chain.chain_lock();
  net_data_obj *info =
      (net_data_obj *)clientInfo_chain.chain_get_by_ident(sock_descriptor);
  if (info != NULL && info->ident == cltid)
    fd = fcntl(sock_descriptor, F_DUPFD_CLOEXEC, 0);
  clientInfo_chain.chain_unlock();
  return fd;
}
// sends the whole buffer; handles partial writes, EINTR and EAGAIN until
// AD_NET_SERVER_SEND_TIMEOUT_MS expired
int ADNetServer::send_with_deadline(int fd, const char *buf, int len) {
  int sent = 0;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long deadline =
      ts.tv_sec * 1000L + ts.tv_nsec / 1000000L + AD_NET_SERVER_SEND_TIMEOUT_MS;
  while (sent < len) {
    ssize_t rc = send(fd, buf + sent, len - sent, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (rc > 0) {
      sent += rc;
      continue;
    }
    if (rc < 0 && errno == EINTR)
      continue;
    if (rc < 0 && !would_block(errno))
      return -1;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long left = deadline - (ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
    if (left <= 0)
      return -1;
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;
    if (poll(&pfd, 1, (int)left) < 0 && errno != EINTR)
      return -1;
  }
  return 0;
}
bool ADNetServer::IsConnectionAlive(int sock_descriptor) {
  socklen_t len;
  struct sockaddr_storage peer;
  len = sizeof peer;
  if (getpeername(sock_descriptor, (struct sockaddr *)&peer, &len) == 0) {
    return true;
  } else {
    return false;
  }
}
int ADNetServer::monoshot_callback_function(void *pUserData,
                                            ADThreadProducer *pObj) {
  int call_from = pObj->getID();
  if (call_from == id_response_thread) {
    net_data_obj *resp_obj;
    // drain everything queued; wakeups may be coalesced
    while ((resp_obj = (net_data_obj *)response_chain.chain_get()) != NULL) {
      int fd = dup_if_same_client(resp_obj->sock_descriptor, resp_obj->cltid);
      if (fd >= 0) {
        if (send_with_deadline(fd, resp_obj->data_buffer,
                               resp_obj->data_buffer_len) != 0 &&
            socketlog)
          printf("[%06d]response dropped (send failed or timed out)\n",
                 resp_obj->sock_descriptor);
        close(fd);
      }
      ARRAY_MEM_DELETE(resp_obj->data_buffer);
      OBJ_MEM_DELETE(resp_obj);
    }
  }
  return 0;
}
int ADNetServer::thread_callback_function(void *pUserData,
                                          ADThreadProducer *pObj) {
  int call_from = pObj->getID();
  if (call_from != id_listen_thread)
    return 0;
  struct sockaddr in_addr;
  socklen_t in_len;
  int i, rc, desc_ready, close_conn, new_sd;
  fd_set working_set;
  in_len = sizeof(in_addr);
  do {
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    memcpy(&working_set, &master_set, sizeof(master_set));
    rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);
    if (__atomic_load_n(&end_server, __ATOMIC_SEQ_CST) == AD_NETWORK_TRUE)
      break;
    if (rc < 0) {
      if (errno == EINTR)
        continue; // a signal is not a reason to stop serving
      LOG_ERR_MSG_WITH_ARG("libadav:ADNetServer", "select() failed errno=%d",
                           errno);
      if (errno == EBADF || errno == EINVAL)
        break;
      usleep(100000);
      continue;
    }
    if (rc == 0) {
      continue;
    }
    desc_ready = rc;
    for (i = 0; i <= max_sd && desc_ready > 0; ++i) {
      if (FD_ISSET(i, &working_set)) {
        desc_ready -= 1;
        if (i == wake_pipe[0]) {
          break; // stop_receiving(): end_server is set
        } else if (i == listen_sd) {
          for (;;) {
            in_len = sizeof(in_addr);
            new_sd = accept4(listen_sd, &in_addr, &in_len, SOCK_CLOEXEC);
            if (new_sd < 0) {
              if (errno == EINTR || errno == ECONNABORTED)
                continue; // retry
              if (!would_block(errno)) {
                // EMFILE/ENFILE/ENOBUFS/ENOMEM: transient, keep serving
                LOG_ERR_MSG_WITH_ARG("libadav:ADNetServer",
                                     "accept() failed errno=%d", errno);
                if (errno == EBADF || errno == EINVAL || errno == ENOTSOCK) {
                  __atomic_store_n(&end_server, (unsigned char)AD_NETWORK_TRUE,
                                   __ATOMIC_SEQ_CST);
                } else
                  usleep(100000); // back off; connection stays queued
              }
              break;
            }
            if (new_sd >= FD_SETSIZE) {
              // FD_SET() beyond FD_SETSIZE corrupts memory: refuse the client
              LOG_ERR_MSG_WITH_ARG("libadav:ADNetServer",
                                   "rejecting client fd=%d >= FD_SETSIZE",
                                   new_sd);
              close(new_sd);
              continue;
            }
            print_client_info(&in_addr, in_len, new_sd);
            if (sock_type == ADLIB_TCP_SOCKET_TYPE_JSON) {
              delete framers[new_sd]; // stale entry, must not happen
              framers[new_sd] =
                  new ADJsonStreamFramer(AD_NET_SERVER_MAX_JSON_MSG_SIZE);
            }
            FD_SET(new_sd, &master_set);
            if (new_sd > max_sd)
              max_sd = new_sd;
          }
        } else {
          close_conn = AD_NETWORK_FALSE;
          do {
            // leave room for the terminating NUL
            rc = recv(i, receive_buffer, sizeof(receive_buffer) - 1,
                      MSG_DONTWAIT);
            if (rc < 0) {
              if (errno == EINTR)
                continue;
              if (!would_block(errno)) {
                close_conn = AD_NETWORK_TRUE;
              }
              break;
            }
            if (rc == 0) {
              if (socketlog)
                printf("[%06d]Connection closed\n", i);
              close_conn = AD_NETWORK_TRUE;
              break;
            }
            receive_size = rc;
            receive_buffer[receive_size] = '\0';
            switch (sock_type) {
            case ADLIB_TCP_SOCKET_TYPE_JSON:
              if (json_receive_data_and_notify_consumer(i, receive_buffer,
                                                        receive_size) < 0) {
                // the stream cannot be resynchronized: tell the client
                // and drop the connection
                send_protocol_error(i);
                close_conn = AD_NETWORK_TRUE;
              }
              break;
            default:
              binary_receive_data_and_notify_consumer(i, receive_buffer,
                                                      receive_size);
              break;
            }
          } while (close_conn == AD_NETWORK_FALSE);
          if (close_conn) {
            close_connection(i);
            FD_CLR(i, &master_set);
            if (i == max_sd) {
              while (max_sd > 0 &&
                     FD_ISSET(max_sd, &master_set) == AD_NETWORK_FALSE)
                max_sd -= 1;
            }
          }
        }
      }
    }
  } while (__atomic_load_n(&end_server, __ATOMIC_SEQ_CST) == AD_NETWORK_FALSE);
  return 0;
}
ADNetServer::ADNetServer() {
  wake_pipe[0] = wake_pipe[1] = -1;
  bind_address = htonl(INADDR_ANY);
  socketlog = 0;
  connected = 0;
  listen_port = AD_NET_SERVER_DEFAULT_LISTEN_PORT;
  end_server = AD_NETWORK_FALSE;
  sock_type = ADLIB_TCP_SOCKET_TYPE_JSON;
  initialize_helpers();
}
ADNetServer::ADNetServer(int port) {
  wake_pipe[0] = wake_pipe[1] = -1;
  bind_address = htonl(INADDR_ANY);
  socketlog = 0;
  connected = 0;
  listen_port = port;
  end_server = AD_NETWORK_FALSE;
  sock_type = ADLIB_TCP_SOCKET_TYPE_JSON;
  initialize_helpers();
}
ADNetServer::~ADNetServer() { stop_listening(); }
int ADNetServer::set_bind_address(const char *ip) {
  struct in_addr a;
  if (ip == NULL || inet_pton(AF_INET, ip, &a) != 1)
    return -1;
  bind_address = a.s_addr;
  return 0;
}
int ADNetServer::start_listening(int port, int socket_log,
                                 ADLIB_TCP_SOCKET_TYPE socket_type) {
  socketlog = socket_log;
  listen_port = port;
  sock_type = socket_type;
  return start_listening();
}
// logs a start-up failure to syslog (with errno) and stdout
static int start_failed(const char *what, int fd, int port) {
  int err = errno;
  char msg[160];
  snprintf(msg, sizeof(msg), "listen on port %d: %s failed: %s", port, what,
           strerror(err));
  LOG_ERR_MSG_WITH_ARG("libadav:ADNetServer", "%s", msg);
  printf("ADNetServer: %s\n", msg);
  if (fd >= 0)
    close(fd);
  errno = err;
  return -1;
}
int ADNetServer::start_listening() {
  int rc;
  int on = 1;
  if (connected)
    return -1; // already listening
  // close-on-exec: children started with popen()/system() must not inherit
  // the listening socket (finding V2-H1)
  listen_sd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listen_sd < 0)
    return start_failed("socket()", -1, listen_port);
  rc = setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
  if (rc < 0)
    return start_failed("setsockopt()", listen_sd, listen_port);
  rc = ioctl(listen_sd, FIONBIO, (char *)&on);
  if (rc < 0)
    return start_failed("ioctl()", listen_sd, listen_port);
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = bind_address;
  addr.sin_port = htons(listen_port);
  rc = bind(listen_sd, (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0)
    return start_failed("bind()", listen_sd, listen_port);
  rc = listen(listen_sd, 32);
  if (rc < 0)
    return start_failed("listen()", listen_sd, listen_port);
  if (pipe2(wake_pipe, O_CLOEXEC) != 0)
    return start_failed("pipe()", listen_sd, listen_port);
  FD_ZERO(&master_set);
  max_sd = listen_sd > wake_pipe[0] ? listen_sd : wake_pipe[0];
  FD_SET(listen_sd, &master_set);
  FD_SET(wake_pipe[0], &master_set);
  end_server = AD_NETWORK_FALSE;
  response_thread.start_thread();
  listen_thread.start_thread();
  connected = 1;
  return 0;
}
int ADNetServer::stop() { return stop_listening(); }
int ADNetServer::stop_receiving() {
  if (connected == 0)
    return 0;
  __atomic_store_n(&end_server, (unsigned char)AD_NETWORK_TRUE,
                   __ATOMIC_SEQ_CST);
  char wake = 1;
  if (write(wake_pipe[1], &wake, 1) < 0) {
    ; // the 1 s select timeout ends the loop anyway
  }
  listen_thread.stop_thread();
  return 0;
}
int ADNetServer::stop_listening() {
  if (connected == 0)
    return 0;
  // cooperative stop: input first, then the response sender
  stop_receiving();
  response_thread.stop_thread();
  request_chain.remove_all();
  response_chain.remove_all();
  clientInfo_chain.remove_all();
  for (int i = 0; i <= max_sd; ++i) {
    if (FD_ISSET(i, &master_set)) {
      if (i == listen_sd || i == wake_pipe[0])
        close(i);
      else
        close_connection(i);
    }
  }
  close(wake_pipe[1]);
  connected = 0;
  return 0;
}
int ADNetServer::initialize_helpers(void) {
  request_chain.attach_helper(this);
  request_chain.set_element_deleter(&chain_delete_object<net_data_obj>);
  response_chain.attach_helper(this);
  response_chain.set_element_deleter(&chain_delete_object<net_data_obj>);
  clientInfo_chain.attach_helper(this);
  clientInfo_chain.set_element_deleter(&chain_delete_object<net_data_obj>);
  id_listen_thread = listen_thread.subscribe_thread_callback(this);
  id_response_thread = response_thread.subscribe_thread_callback(this);
  listen_thread.set_thread_properties(THREAD_TYPE_NOBLOCK, (void *)this);
  response_thread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  return 0;
}
int ADNetServer::print_client_info(struct sockaddr *in_addr, socklen_t in_len,
                                   int sock_descr) {
  int port;
  int in_clt_info;
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  in_clt_info = getnameinfo(in_addr, in_len, hbuf, sizeof(hbuf), sbuf,
                            sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
  if (in_clt_info == 0) {
    if (socketlog)
      printf("[%06d]Connection accepted: clientip=%s, port=%s\n", sock_descr,
             hbuf, sbuf);
    port = atoi(sbuf);
    register_client_info(sock_descr, port, hbuf);
  } else {
    if (socketlog)
      printf("getnameinfo returned = %d\n", in_clt_info);
    // still register: the record identifies the connection (cltid)
    register_client_info(sock_descr, -1, (char *)"");
  }
  return 0;
}
int ADNetServer::register_client_info(int sock_descr, int clt_port,
                                      char *clt_ip) {
  net_data_obj *resp_obj = NULL;
  OBJECT_MEM_NEW(resp_obj, net_data_obj);
  if (resp_obj == NULL)
    return -1;
  resp_obj->ident = clientInfo_chain.chain_generate_ident();
  resp_obj->sock_descriptor = sock_descr;
  resp_obj->port = clt_port;
  resp_obj->data_buffer = NULL;
  snprintf(resp_obj->ip, sizeof(resp_obj->ip), "%s", clt_ip);
  if (clientInfo_chain.chain_put((void *)resp_obj) != 0) {
    printf("failed! unable to push client-info object to chain!\n");
    OBJ_MEM_DELETE(resp_obj);
    return -1;
  }
  return 0;
}
int ADNetServer::deregister_client_info(int sock_descr) {
  net_data_obj *info_obj = NULL;
  while ((info_obj = (net_data_obj *)clientInfo_chain.chain_remove_by_ident(
              sock_descr)) != NULL)
    OBJ_MEM_DELETE(info_obj);
  return 0;
}
int ADNetServer::get_client_info(int sock_descr, char *cltip, int *cltport,
                                 int *cltid) {
  net_data_obj *info_obj = NULL;
  int ret = -1;
  clientInfo_chain.chain_lock();
  info_obj = (net_data_obj *)clientInfo_chain.chain_get_by_ident(sock_descr);
  if (info_obj != NULL) {
    strcpy(cltip, info_obj->ip);
    *cltport = info_obj->port;
    *cltid = info_obj->ident;
    ret = 0;
  }
  clientInfo_chain.chain_unlock();
  return ret;
}
int ADNetServer::schedule_response(int socket_descriptor, char *buf, int len) {
  return schedule_response(socket_descriptor, -1, buf, len);
}
int ADNetServer::schedule_response(int socket_descriptor, int cltid, char *buf,
                                   int len) {
  net_data_obj *resp_obj = NULL;
  OBJECT_MEM_NEW(resp_obj, net_data_obj);
  if (resp_obj == NULL)
    return -1;
  resp_obj->ident = 0;
  resp_obj->cltid = cltid;
  resp_obj->sock_descriptor = socket_descriptor;
  resp_obj->data_buffer_len = len;
  resp_obj->data_buffer = NULL;
  ARRAY_MEM_NEW(resp_obj->data_buffer, (len + 2));
  if (resp_obj->data_buffer == NULL) {
    OBJ_MEM_DELETE(resp_obj);
    return -1;
  }
  memcpy(resp_obj->data_buffer, buf, len);
  resp_obj->data_buffer[len] = '\0';
  if (response_chain.chain_put((void *)resp_obj) != 0) {
    printf("failed! unable to push response object to chain!\n");
    ARRAY_MEM_DELETE(resp_obj->data_buffer);
    OBJ_MEM_DELETE(resp_obj);
    return -1;
  }
  response_thread.wakeup_thread();
  return 0;
}
int ADNetServer::binary_receive_data_and_notify_consumer(int socket_descriptor,
                                                         char *buf, int len) {
  char cltip[512];
  int cltport = -1;
  int cltid = -1;
  if (get_client_info(socket_descriptor, cltip, &cltport, &cltid) != 0)
    cltip[0] = '\0';
  net_data_obj *resp_obj = NULL;
  OBJECT_MEM_NEW(resp_obj, net_data_obj);
  if (resp_obj == NULL)
    return -1;
  resp_obj->ident = request_chain.chain_generate_ident();
  resp_obj->sock_descriptor = socket_descriptor;
  resp_obj->port = cltport;
  strcpy(resp_obj->ip, cltip);
  resp_obj->cltid = cltid;
  resp_obj->data_buffer_len = len;
  ARRAY_MEM_NEW(resp_obj->data_buffer, (len + 2));
  if (resp_obj->data_buffer == NULL) {
    OBJ_MEM_DELETE(resp_obj);
    return -1;
  }
  memcpy(resp_obj->data_buffer, buf, len);
  resp_obj->data_buffer[len] = '\0';
  if (request_chain.chain_put((void *)resp_obj) != 0) {
    printf("failed! unable to push response object to chain!\n");
    ARRAY_MEM_DELETE(resp_obj->data_buffer);
    OBJ_MEM_DELETE(resp_obj);
    return -1;
  }
  notify_data_arrival(&request_chain);
  return 0;
}
// Feeds the bytes into the connection's framer and queues every complete
// request. Returns -1 on a framing error (garbage between messages or a
// request above AD_NET_SERVER_MAX_JSON_MSG_SIZE).
int ADNetServer::json_receive_data_and_notify_consumer(int socket_descriptor,
                                                       char *buf, int len) {
  char cltip[512];
  int cltport = -1;
  int cltid = -1;
  std::map<int, ADJsonStreamFramer *>::iterator it =
      framers.find(socket_descriptor);
  if (it == framers.end())
    return -1;
  int frc = it->second->feed(buf, len);
  if (it->second->available() == 0)
    return frc < 0 ? -1 : 0;
  if (get_client_info(socket_descriptor, cltip, &cltport, &cltid) != 0)
    cltip[0] = '\0';
  std::string msg;
  int queued = 0;
  while (it->second->next(msg)) {
    net_data_obj *resp_obj = NULL;
    OBJECT_MEM_NEW(resp_obj, net_data_obj);
    if (resp_obj == NULL)
      break;
    resp_obj->ident = request_chain.chain_generate_ident();
    resp_obj->sock_descriptor = socket_descriptor;
    resp_obj->port = cltport;
    strcpy(resp_obj->ip, cltip);
    resp_obj->cltid = cltid;
    resp_obj->data_buffer_len = msg.size();
    ARRAY_MEM_NEW(resp_obj->data_buffer, msg.size() + 1);
    if (resp_obj->data_buffer == NULL) {
      OBJ_MEM_DELETE(resp_obj);
      break;
    }
    memcpy(resp_obj->data_buffer, msg.data(), msg.size());
    resp_obj->data_buffer[msg.size()] = '\0';
    if (request_chain.chain_put((void *)resp_obj) != 0) {
      printf("failed! unable to push request object to chain!\n");
      ARRAY_MEM_DELETE(resp_obj->data_buffer);
      OBJ_MEM_DELETE(resp_obj);
      break;
    }
    queued++;
  }
  // requests of one connection reach the consumer in arrival order: one
  // listen thread pushes them to one FIFO chain
  if (queued > 0)
    notify_data_arrival(&request_chain);
  return frc < 0 ? -1 : 0;
}
void ADNetServer::send_protocol_error(int socket_descriptor) {
  static const char err[] = "{ \"jsonrpc\": \"2.0\", \"error\": { \"code\": "
                            "-32700, \"message\": \"Parse error.\" }, "
                            "\"id\": null }";
  if (socketlog)
    printf("[%06d]framing error, closing connection\n", socket_descriptor);
  send(socket_descriptor, err, sizeof(err) - 1, MSG_DONTWAIT | MSG_NOSIGNAL);
}
// every close path goes through here: the client record and the framer of
// this fd must not survive, the fd number can be reused by the next accept
void ADNetServer::close_connection(int socket_descriptor) {
  deregister_client_info(socket_descriptor);
  std::map<int, ADJsonStreamFramer *>::iterator it =
      framers.find(socket_descriptor);
  if (it != framers.end()) {
    delete it->second;
    framers.erase(it);
  }
  close(socket_descriptor);
}
