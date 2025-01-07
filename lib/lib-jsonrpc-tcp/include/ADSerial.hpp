#ifndef __ADSERIAL_H_
#define __ADSERIAL_H_
#define DEV_NODE_NAME_MAX_SIZE 512
#include "ADGenericChain.hpp"
#include "ADThread.hpp"
#include "ADTimer.hpp"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#define MAX_RX_TX_CHUNK_SZ 4096
typedef struct rx_tx_data_t {
  int ident;
  int len;
  unsigned char *RxTxdata;
} rx_tx_data;
class ADSerialProducer;
class ADCSerialConsumer {
public:
  virtual int notify_data_arrival(ADGenericChain *pRxChain,
                                  ADSerialProducer *pObj) = 0;
  virtual ~ADCSerialConsumer(){};
};
class ADSerialProducer {
  static int IDGenerator;
  ADCSerialConsumer *pConsumer;
  int id;

protected:
  int notify_data_arrival(ADGenericChain *pRxChain) {
    if (pConsumer != NULL)
      return pConsumer->notify_data_arrival(pRxChain, this);
    return -1;
  }
  int is_helper_attached(void) {
    if (pConsumer == NULL)
      return -1;
    return 0;
  }

public:
  ADSerialProducer() {
    id = IDGenerator++;
    pConsumer = NULL;
  }
  virtual ~ADSerialProducer(){};
  int subscribe_data_arrival(ADCSerialConsumer *c) {
    if (pConsumer == NULL) {
      pConsumer = c;
      return id;
    } else
      return -1;
  }
  int getID() { return id; }
};
class ADSerial : public ADTimerConsumer,
                 public ADChainConsumer,
                 public ADThreadConsumer,
                 public ADSerialProducer {
  pthread_mutex_t multi_callback_lock;
  int transmission_in_progress;
  int timer_event_attached;
  int connected;
  int baud;
  int converted_baud;
  char dev_node[DEV_NODE_NAME_MAX_SIZE];
  int fd;
  int uartlog;
  struct termios old_options;
  char uart_timestamp[255];
  int id_rx_chain, id_tx_chain, id_rx_thread, id_tx_thread,
      id_rx_data_proc_thread;
  ADGenericChain rx_chain;
  ADGenericChain tx_chain;
  ADThread rx_thread;
  ADThread tx_thread;
  ADThread rx_data_proc_thread;
  virtual int timer_notification();
  virtual int sigio_notification();
  virtual int custom_sig_notification(int signum);
  virtual int identify_chain_element(void *element, int ident,
                                     ADChainProducer *pObj);
  virtual int double_identify_chain_element(void *element, int ident1,
                                            int ident2, ADChainProducer *pObj);
  virtual int free_chain_element_data(void *element, ADChainProducer *pObj);
  virtual int monoshot_callback_function(void *pUserData,
                                         ADThreadProducer *pObj);
  virtual int thread_callback_function(void *pUserData, ADThreadProducer *pObj);
  int convert_baud(int baudrate);
  int initialize_helpers(void);
  int init_serial_port();
  int push_data_into_chain(ADGenericChain *pChain, unsigned char *data,
                           int len);
  int process_received_data(unsigned char *data, int len);
  int read_serial_data(int fd, unsigned char *buf, int size);
  int write_serial_data(int fd, unsigned char *data, int bytesToSend);
  char *get_timestamp();

public:
  int us_data_time;
  ADSerial();
  ADSerial(int baud_rate, char *dev_node_name);
  ~ADSerial();
  int set_serial_port_settings(int baud_rate, char *device);
  int start();
  int stop();
  int schedule_data_transmission(unsigned char *data, int len);
  int attach_timer_and_signal_notifier(ADTimer *pTimer);
  void set_uart_logging(int log);
  int get_uart_logging();
};
#endif
