#include "ADSerial.hpp"
#include "ADCommon.hpp"
#include <iostream>
#include <stdio.h>
#include <string.h>
using namespace std;
int ADSerialProducer::IDGenerator = 0;
int ADSerial::timer_notification() {
  if (connected == 0)
    return -1;
  return 0;
}
int ADSerial::sigio_notification() {
  if (connected == 0)
    return -1;
  rx_thread.wakeup_thread();
  return 0;
}
int ADSerial::custom_sig_notification(int signum) { return 0; }
int ADSerial::identify_chain_element(void *element, int ident,
                                     ADChainProducer *pObj) {
  return -1;
}
int ADSerial::double_identify_chain_element(void *element, int ident1,
                                            int ident2, ADChainProducer *pObj) {
  return -1;
}
int ADSerial::free_chain_element_data(void *element, ADChainProducer *pObj) {
  rx_tx_data *objData;
  objData = (rx_tx_data *)element;
  if (objData->RxTxdata != NULL)
    MEM_DELETE(objData->RxTxdata);
  return 0;
}
int ADSerial::monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj) {
  pthread_mutex_lock(&multi_callback_lock);
  int call_from = pObj->getID();
  if (call_from == id_rx_thread) {
    int data_len;
    unsigned char buf[MAX_RX_TX_CHUNK_SZ];
    data_len = read_serial_data(fd, buf, MAX_RX_TX_CHUNK_SZ - 1);
    if (data_len < 1) {
      pthread_mutex_unlock(&multi_callback_lock);
      return 0;
    }
    if (uartlog) {
      printf("%s:rx <== ", get_timestamp());
      for (int i = 0; i < data_len; i++)
        printf("%c", buf[i]);
      printf("\n");
      printf("             rxH<== ");
      for (int j = 0; j < data_len; j++)
        printf("%02X ", buf[j]);
      printf("\n");
    }
    process_received_data(buf, data_len);
    pthread_mutex_unlock(&multi_callback_lock);
    return 0;
  } else if (call_from == id_tx_thread) {
    rx_tx_data *rt_data;
    rt_data = (rx_tx_data *)tx_chain.chain_get();
    if (rt_data != NULL) {
      write_serial_data(fd, rt_data->RxTxdata, rt_data->len);
      MEM_DELETE(rt_data->RxTxdata);
      MEM_DELETE(rt_data);
    }
    pthread_mutex_unlock(&multi_callback_lock);
    return 0;
  } else if (call_from == id_rx_data_proc_thread) {
    notify_data_arrival(&rx_chain);
  }
  pthread_mutex_unlock(&multi_callback_lock);
  return 0;
}
int ADSerial::thread_callback_function(void *pUserData,
                                       ADThreadProducer *pObj) {
  cout << "ADAVClass::thread_callback_function() called id=" << pObj->getID()
       << endl;
  return 0;
}
#include <signal.h>
void SIOHandler(int num) {
  printf("SDSRV:AdLib:ADSerial: Ouch!!! received SIGIO\n");
}
ADSerial::ADSerial() {
  pthread_mutex_init(&multi_callback_lock, NULL);
  transmission_in_progress = 0;
  timer_event_attached = 0;
  connected = 0;
  baud = 9600;
  strcpy(dev_node, "/dev/ttyS0");
  uartlog = 0;
  initialize_helpers();
  signal(SIGIO, SIOHandler);
}
ADSerial::ADSerial(int baud_rate, char *dev_node_name) {
  pthread_mutex_init(&multi_callback_lock, NULL);
  transmission_in_progress = 0;
  timer_event_attached = 0;
  connected = 0;
  baud = baud_rate;
  if (strlen(dev_node_name) < DEV_NODE_NAME_MAX_SIZE)
    strcpy(dev_node, dev_node_name);
  else
    strcpy(dev_node, "/dev/ttyS0");
  initialize_helpers();
  signal(SIGIO, SIOHandler);
}
ADSerial::~ADSerial() { stop(); }
int ADSerial::set_serial_port_settings(int baud_rate, char *device) {
  baud = baud_rate;
  strcpy(dev_node, device);
  return 0;
}
int ADSerial::initialize_helpers(void) {
  id_rx_chain = rx_chain.attach_helper(this);
  id_tx_chain = tx_chain.attach_helper(this);
  id_rx_thread = rx_thread.subscribe_thread_callback(this);
  id_tx_thread = tx_thread.subscribe_thread_callback(this);
  id_rx_data_proc_thread = rx_data_proc_thread.subscribe_thread_callback(this);
  rx_thread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  tx_thread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  rx_data_proc_thread.set_thread_properties(THREAD_TYPE_MONOSHOT, (void *)this);
  return 0;
}
int ADSerial::attach_timer_and_signal_notifier(ADTimer *pTimer) {
  pTimer->subscribe_timer_notification(this);
  timer_event_attached = 1;
  return 0;
}
int ADSerial::start() {
  if (connected == 1)
    return -1;
  if (timer_event_attached == 0)
    return -1;
  converted_baud = convert_baud(baud);
  if (converted_baud < 0)
    return -1;
  fd = open(dev_node, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
  if (fd < 0)
    return -1;
  rx_thread.start_thread();
  tx_thread.start_thread();
  rx_data_proc_thread.start_thread();
  return init_serial_port();
}
int ADSerial::init_serial_port() {
  int error;
  struct termios options;
  fcntl(fd, F_SETFL, FASYNC);
  tcgetattr(fd, &old_options);
  memset(&options, 0, sizeof(options));
  options.c_cflag = converted_baud | CS8 | CLOCAL | CREAD;
  options.c_iflag = IGNPAR;
  options.c_oflag = 0;
  options.c_lflag = 0;
  options.c_cc[VMIN] = 0;
  options.c_cc[VTIME] = 0;
  error = tcsetattr(fd, TCSANOW, &options);
  if (error == -1) {
    close(fd);
    return -1;
  }
  connected = 1;
  cout << "Serial Port " << dev_node << " @" << baud
       << " initialized successfully" << endl;
  return 0;
}
int ADSerial::stop() {
  if (connected == 0)
    return 0;
  rx_data_proc_thread.stop_thread();
  rx_thread.stop_thread();
  tx_thread.stop_thread();
  rx_chain.remove_all();
  tx_chain.remove_all();
  tcflush(fd, TCIOFLUSH);
  tcsetattr(fd, TCSANOW, &old_options);
  close(fd);
  connected = 0;
  return 0;
}
int ADSerial::convert_baud(int baudrate) {
  int baudr;
  switch (baudrate) {
  case 50:
    baudr = B50;
    break;
  case 75:
    baudr = B75;
    break;
  case 110:
    baudr = B110;
    break;
  case 134:
    baudr = B134;
    break;
  case 150:
    baudr = B150;
    break;
  case 200:
    baudr = B200;
    break;
  case 300:
    baudr = B300;
    break;
  case 600:
    baudr = B600;
    break;
  case 1200:
    baudr = B1200;
    break;
  case 1800:
    baudr = B1800;
    break;
  case 2400:
    baudr = B2400;
    break;
  case 4800:
    baudr = B4800;
    break;
  case 9600:
    baudr = B9600;
    us_data_time = 100000;
    break;
  case 19200:
    baudr = B19200;
    break;
  case 38400:
    baudr = B38400;
    break;
  case 57600:
    baudr = B57600;
    break;
  case 115200:
    baudr = B115200;
    us_data_time = 1000;
    break;
  case 230400:
    baudr = B230400;
    break;
  case 460800:
    baudr = B460800;
    break;
  case 500000:
    baudr = B500000;
    break;
  case 576000:
    baudr = B576000;
    break;
  case 921600:
    baudr = B921600;
    break;
  case 1000000:
    baudr = B1000000;
    break;
  default:
    printf("invalid baudrate\n");
    return -1;
    break;
  }
  return baudr;
}
int ADSerial::push_data_into_chain(ADGenericChain *pChain, unsigned char *data,
                                   int len) {
  rx_tx_data *data_obj = NULL;
  if (len > MAX_RX_TX_CHUNK_SZ)
    return -1;
  OBJECT_MEM_CREATE(data_obj, rx_tx_data);
  if (data_obj == NULL)
    return -1;
  data_obj->ident = pChain->chain_generate_ident();
  data_obj->len = len;
  data_obj->RxTxdata = NULL;
  data_obj->RxTxdata = (unsigned char *)malloc(len + 2);
  if (data_obj->RxTxdata == NULL) {
    MEM_DELETE(data_obj);
    return -1;
  }
  memcpy(data_obj->RxTxdata, data, len);
  if (pChain->chain_put((void *)data_obj) != 0) {
    printf("failed! unable to push data_obj to chain!\n");
    MEM_DELETE(data_obj->RxTxdata);
    MEM_DELETE(data_obj);
    return -1;
  }
  return 0;
}
int ADSerial::schedule_data_transmission(unsigned char *data, int len) {
  if (push_data_into_chain(&tx_chain, data, len) != 0)
    return -1;
  tx_thread.wakeup_thread();
  return 0;
}
int ADSerial::process_received_data(unsigned char *data, int len) {
  if (push_data_into_chain(&rx_chain, data, len) != 0)
    return -1;
  rx_data_proc_thread.wakeup_thread();
  return 0;
}
int ADSerial::read_serial_data(int fd, unsigned char *buf, int size) {
  int n;
#ifndef __STRICT_ANSI__
  if (size > SSIZE_MAX)
    size = (int)SSIZE_MAX;
#else
  if (size > MAX_RX_TX_CHUNK_SZ)
    size = MAX_RX_TX_CHUNK_SZ;
#endif
  n = read(fd, buf, size);
  return (n);
}
int ADSerial::write_serial_data(int fd, unsigned char *data, int bytesToSend) {
  int writtenBytes = 0;
  int lastBytesWritten = 0;
  if (uartlog) {
    printf("%s:tx ==> ", get_timestamp());
    for (int i = 0; i < bytesToSend; i++)
      printf("%c", data[i]);
    printf("\n");
    printf("             txH==> ");
    for (int j = 0; j < bytesToSend; j++)
      printf("%02X ", data[j]);
    printf("\n");
  }
  tcflush(fd, TCIOFLUSH);
  transmission_in_progress = 1;
  while (writtenBytes < bytesToSend) {
    lastBytesWritten =
        write(fd, data + writtenBytes, bytesToSend - writtenBytes);
    writtenBytes += lastBytesWritten;
    if (lastBytesWritten < 0) {
      printf("write failed!\n");
      return -1;
    }
  }
  return 0;
}
void ADSerial::set_uart_logging(int log) { uartlog = log; }
int ADSerial::get_uart_logging() { return uartlog; }
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
char *ADSerial::get_timestamp() {
  char buffer[30];
  struct timeval tv;
  time_t curtime;
  uart_timestamp[0] = '\0';
  gettimeofday(&tv, NULL);
  curtime = tv.tv_sec;
  strftime(buffer, 30, "%T.", localtime(&curtime));
  sprintf(uart_timestamp, "%s%03ld", buffer, (tv.tv_usec / 1000));
  return uart_timestamp;
}
