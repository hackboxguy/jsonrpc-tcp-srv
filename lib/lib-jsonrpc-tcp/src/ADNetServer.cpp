#include "ADNetServer.hpp"
#include "ADCommon.hpp"
#include <iostream>
using namespace std;
#define AD_NETWORK_TRUE 1
#define AD_NETWORK_FALSE 0
int ADNetProducer::IDGenerator = 0;
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
  OBJ_MEM_DELETE(obj->data_buffer);
  return 0;
}
bool ADNetServer::IsConnectionAlive(int sock_descriptor) {
  socklen_t len;
  struct sockaddr_storage addr;
  len = sizeof addr;
  if (getpeername(sock_descriptor, (struct sockaddr *)&addr, &len) == 0) {
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
    resp_obj = (net_data_obj *)response_chain.chain_get();
    if (resp_obj != NULL) {
      if (IsConnectionAlive(resp_obj->sock_descriptor))
        send(resp_obj->sock_descriptor, resp_obj->data_buffer,
             resp_obj->data_buffer_len, MSG_DONTWAIT | MSG_NOSIGNAL);
      OBJ_MEM_DELETE(resp_obj->data_buffer);
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
    if (end_server == AD_NETWORK_TRUE)
      break;
    if (rc < 0) {
      printf("  select() failed");
      break;
    }
    if (rc == 0) {
      continue;
    }
    desc_ready = rc;
    for (i = 0; i <= max_sd && desc_ready > 0; ++i) {
      if (FD_ISSET(i, &working_set)) {
        desc_ready -= 1;
        if (i == listen_sd) {
          do {
            new_sd = accept(listen_sd, &in_addr, &in_len);
            if (new_sd < 0) {
              if (errno != EWOULDBLOCK) {
                printf("  accept() failed");
                end_server = AD_NETWORK_TRUE;
              }
              break;
            }
            print_client_info(&in_addr, in_len, new_sd);
            FD_SET(new_sd, &master_set);
            if (new_sd > max_sd)
              max_sd = new_sd;
          } while (new_sd != -1);
        } else {
          close_conn = AD_NETWORK_FALSE;
          do {
            rc = recv(i, receive_buffer, sizeof(receive_buffer), MSG_DONTWAIT);
            if (rc < 0) {
              if (errno != EWOULDBLOCK) {
                close_conn = AD_NETWORK_TRUE;
              }
              break;
            }
            if (rc == 0) {
              if (socketlog)
                printf("[%06d]Connection closed\n", i);
              deregister_client_info(i);
              close_conn = AD_NETWORK_TRUE;
              break;
            }
            receive_size = rc;
            receive_buffer[receive_size] = '\0';
            switch (sock_type) {
            case ADLIB_TCP_SOCKET_TYPE_JSON:
              json_receive_data_and_notify_consumer(i, receive_buffer,
                                                    receive_size);
              break;
            default:
              binary_receive_data_and_notify_consumer(i, receive_buffer,
                                                      receive_size);
              break;
            }
          } while (AD_NETWORK_TRUE);
          if (close_conn) {
            close(i);
            FD_CLR(i, &master_set);
            if (i == max_sd) {
              while (FD_ISSET(max_sd, &master_set) == AD_NETWORK_FALSE)
                max_sd -= 1;
            }
          }
        }
      }
    }
  } while (end_server == AD_NETWORK_FALSE);
  return 0;
}
#include <signal.h>
void Ouch(int num) {
  LOG_ERR_MSG("ADNetServer:AdLib", "Ouch!!! received SIGALRM");
}
ADNetServer::ADNetServer() {
  receive_size_pending = 0;
  socketlog = 0;
  connected = 0;
  listen_port = AD_NET_SERVER_DEFAULT_LISTEN_PORT;
  end_server = AD_NETWORK_FALSE;
  sock_type = ADLIB_TCP_SOCKET_TYPE_JSON;
  initialize_helpers();
  signal(SIGALRM, Ouch);
}
ADNetServer::ADNetServer(int port) {
  receive_size_pending = 0;
  socketlog = 0;
  connected = 0;
  listen_port = port;
  end_server = AD_NETWORK_FALSE;
  sock_type = ADLIB_TCP_SOCKET_TYPE_JSON;
  initialize_helpers();
  signal(SIGALRM, Ouch);
}
ADNetServer::~ADNetServer() { stop_listening(); }
int ADNetServer::start_listening(int port, int socket_log,
                                 ADLIB_TCP_SOCKET_TYPE socket_type) {
  socketlog = socket_log;
  listen_port = port;
  sock_type = socket_type;
  return start_listening();
}
int ADNetServer::start_listening() {
  int rc;
  int on = 1;
  listen_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sd < 0) {
    printf("socket() failed");
    return -1;
  }
  rc = setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
  if (rc < 0) {
    printf("setsockopt() failed");
    close(listen_sd);
    return -1;
  }
  rc = ioctl(listen_sd, FIONBIO, (char *)&on);
  if (rc < 0) {
    printf("ioctl() failed");
    close(listen_sd);
    return -1;
  }
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(listen_port);
  rc = bind(listen_sd, (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0) {
    printf("bind() failed");
    close(listen_sd);
    return -1;
  }
  rc = listen(listen_sd, 32);
  if (rc < 0) {
    printf("listen() failed");
    close(listen_sd);
    return -1;
  }
  FD_ZERO(&master_set);
  max_sd = listen_sd;
  FD_SET(listen_sd, &master_set);
  end_server = AD_NETWORK_FALSE;
  response_thread.start_thread();
  listen_thread.start_thread();
  connected = 1;
  return 0;
}
int ADNetServer::stop_listening() {
  if (connected == 0)
    return 0;
  end_server = AD_NETWORK_TRUE;
  response_thread.stop_thread();
  sleep(1);
  listen_thread.stop_thread();
  request_chain.remove_all();
  response_chain.remove_all();
  clientInfo_chain.remove_all();
  for (int i = 0; i <= max_sd; ++i) {
    if (FD_ISSET(i, &master_set))
      close(i);
  }
  connected = 0;
  return 0;
}
int ADNetServer::initialize_helpers(void) {
  request_chain.attach_helper(this);
  response_chain.attach_helper(this);
  clientInfo_chain.attach_helper(this);
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
  int ip_len = strlen(clt_ip);
  if (ip_len > 0 && ip_len < 1023)
    strcpy(resp_obj->ip, clt_ip);
  else
    strcpy(resp_obj->ip, "");
  if (clientInfo_chain.chain_put((void *)resp_obj) != 0) {
    printf("failed! unable to push client-info object to chain!\n");
    OBJ_MEM_DELETE(resp_obj);
    return -1;
  }
  return 0;
}
int ADNetServer::deregister_client_info(int sock_descr) {
  net_data_obj *info_obj = NULL;
  info_obj = (net_data_obj *)clientInfo_chain.chain_remove_by_ident(sock_descr);
  if (info_obj != NULL)
    OBJ_MEM_DELETE(info_obj);
  return 0;
}
int ADNetServer::get_client_info(int sock_descr, char *cltip, int *cltport,
                                 int *cltid) {
  net_data_obj *info_obj = NULL;
  info_obj = (net_data_obj *)clientInfo_chain.chain_get_by_ident(sock_descr);
  if (info_obj != NULL) {
    strcpy(cltip, info_obj->ip);
    *cltport = info_obj->port;
    *cltid = info_obj->ident;
    return 0;
  } else
    return -1;
}
int ADNetServer::schedule_response(int socket_descriptor, char *buf, int len) {
  net_data_obj *resp_obj = NULL;
  OBJECT_MEM_NEW(resp_obj, net_data_obj);
  if (resp_obj == NULL)
    return -1;
  resp_obj->ident = 0;
  resp_obj->sock_descriptor = socket_descriptor;
  resp_obj->data_buffer_len = len;
  resp_obj->data_buffer = NULL;
  ARRAY_MEM_NEW(resp_obj->data_buffer, (len + 2));
  if (resp_obj->data_buffer == NULL) {
    OBJ_MEM_DELETE(resp_obj);
    return -1;
  }
  strcpy(resp_obj->data_buffer, buf);
  if (response_chain.chain_put((void *)resp_obj) != 0) {
    printf("failed! unable to push response object to chain!\n");
    OBJ_MEM_DELETE(resp_obj->data_buffer);
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
  printf("receive_data_and_notify_consumer called\n");
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
  strcpy(resp_obj->data_buffer, buf);
  if (request_chain.chain_put((void *)resp_obj) != 0) {
    printf("failed! unable to push response object to chain!\n");
    OBJ_MEM_DELETE(resp_obj->data_buffer);
    OBJ_MEM_DELETE(resp_obj);
    return -1;
  }
  notify_data_arrival(&request_chain);
  printf("receive_data_and_notify_consumer finished\n");
  return 0;
}
int ADNetServer::json_receive_data_and_notify_consumer(int socket_descriptor,
                                                       char *buf, int len) {
  int last_index = 0;
  char *pTemp = buf;
  char cltip[512];
  int cltport = -1;
  int cltid = -1;
  if (get_client_info(socket_descriptor, cltip, &cltport, &cltid) != 0)
    cltip[0] = '\0';
  for (int i = 0; i < len; i++) {
    if (buf[i] == '}' && buf[i + 1] == '{') {
      net_data_obj *resp_obj = NULL;
      OBJECT_MEM_NEW(resp_obj, net_data_obj);
      if (resp_obj == NULL) {
        return -1;
      }
      resp_obj->ident = request_chain.chain_generate_ident();
      resp_obj->sock_descriptor = socket_descriptor;
      resp_obj->port = cltport;
      strcpy(resp_obj->ip, cltip);
      resp_obj->cltid = cltid;
      resp_obj->data_buffer_len = i - last_index + 1;
      ARRAY_MEM_NEW(resp_obj->data_buffer, (i - last_index + 3));
      if (resp_obj->data_buffer == NULL) {
        OBJ_MEM_DELETE(resp_obj);
        return -1;
      }
      strncpy(resp_obj->data_buffer, pTemp, i - last_index + 1);
      resp_obj->data_buffer[i - last_index + 1] = '\0';
      pTemp = &buf[i + 1];
      if (request_chain.chain_put((void *)resp_obj) != 0) {
        printf("failed! unable to push response object to chain!\n");
        OBJ_MEM_DELETE(resp_obj->data_buffer);
        OBJ_MEM_DELETE(resp_obj);
        return -1;
      }
      last_index = i + 1;
    }
  }
  net_data_obj *resp_obj = NULL;
  OBJECT_MEM_NEW(resp_obj, net_data_obj);
  if (resp_obj == NULL) {
    return -1;
  }
  resp_obj->ident = request_chain.chain_generate_ident();
  resp_obj->sock_descriptor = socket_descriptor;
  resp_obj->port = cltport;
  strcpy(resp_obj->ip, cltip);
  resp_obj->cltid = cltid;
  resp_obj->data_buffer_len = len - last_index;
  ARRAY_MEM_NEW(resp_obj->data_buffer, (len - last_index) + 2);
  if (resp_obj->data_buffer == NULL) {
    OBJ_MEM_DELETE(resp_obj);
    return -1;
  }
  strncpy(resp_obj->data_buffer, pTemp, (len - last_index) + 1);
  if (request_chain.chain_put((void *)resp_obj) != 0) {
    printf("failed! unable to push response object to chain!\n");
    OBJ_MEM_DELETE(resp_obj->data_buffer);
    OBJ_MEM_DELETE(resp_obj);
    return -1;
  }
  notify_data_arrival(&request_chain);
  return 0;
}
int ADNetServer::segmented_json_object(int socket_descriptor, char *buf,
                                       int *len) {
  char *pendingPtr = &receive_buffer_pending[receive_size_pending];
  strcpy(pendingPtr, buf);
  int curly_brace_open = 0;
  int curly_brace_close = 0;
  for (int i = 0; i < (*len + receive_size_pending); i++) {
    if (receive_buffer_pending[i] == '{')
      curly_brace_open++;
    else if (receive_buffer_pending[i] == '}')
      curly_brace_close++;
  }
  if (curly_brace_open != curly_brace_close) {
    receive_size_pending = *len;
    return 1;
  } else {
    strcpy(buf, receive_buffer_pending);
    *len += receive_size_pending;
    receive_size_pending = 0;
  }
  return 0;
}
