#ifndef __ADCHAIN_H_
#define __ADCHAIN_H_
#include <pthread.h>
#include <stdlib.h>
#define SEMA_INIT()                                                            \
  do {                                                                         \
    pthread_mutex_init(&sema, NULL);                                           \
  } while (0)
#define SEMA_LOCK()                                                            \
  do {                                                                         \
    pthread_mutex_lock(&sema);                                                 \
  } while (0)
#define SEMA_UNLOCK()                                                          \
  do {                                                                         \
    pthread_mutex_unlock(&sema);                                               \
  } while (0)
class ADChainProducer;
class ADChainConsumer {
public:
  virtual int identify_chain_element(void *element, int ident,
                                     ADChainProducer *pObj) = 0;
  virtual int double_identify_chain_element(void *element, int ident1,
                                            int ident2,
                                            ADChainProducer *pObj) = 0;
  virtual int free_chain_element_data(void *, ADChainProducer *pObj) = 0;
  virtual ~ADChainConsumer(){};
};
class ADChainProducer {
  static int IDGenerator;
  ADChainConsumer *pConsumer;
  int id;

protected:
  int identify_chain_element(void *element, int ident) {
    if (pConsumer != NULL)
      return pConsumer->identify_chain_element(element, ident, this);
    return -1;
  }
  int double_identify_chain_element(void *element, int ident1, int ident2) {
    if (pConsumer != NULL)
      return pConsumer->double_identify_chain_element(element, ident1, ident2,
                                                      this);
    return -1;
  }
  int free_chain_element_data(void *element) {
    if (pConsumer != NULL)
      return pConsumer->free_chain_element_data(element, this);
    return 0;
  }
  int is_helper_attached(void) {
    if (pConsumer == NULL)
      return -1;
    return 0;
  }

public:
  ADChainProducer() {
    id = IDGenerator++;
    pConsumer = NULL;
  }
  virtual ~ADChainProducer(){};
  int attach_helper(ADChainConsumer *c) {
    if (pConsumer == NULL) {
      pConsumer = c;
      return id;
    } else
      return -1;
  }
  int getID() { return id; }
};
struct chain_holder {
  void *pData;
  struct chain_holder *pPrev;
  struct chain_holder *pNext;
};
class ADGenericChain : public ADChainProducer {
  int disable_autoremove;
  struct chain_holder base_chain;
  int chain_peak_size;
  int chain_size;
  int ident_generator;
  bool init_flag;
  pthread_mutex_t sema;
  int remove();

public:
  ADGenericChain();
  ~ADGenericChain();
  void chain_lock();
  void chain_unlock();
  int chain_put(void *data);
  void *chain_get();
  int chain_add(void *data);
  int chain_remove(void);
  void *chain_get_by_ident(int ident);
  void *chain_remove_by_ident(int ident);
  void *chain_remove_by_double_ident(int ident1, int ident2);
  void *chain_get_by_double_ident(int ident1, int ident2);
  void *chain_get_by_index(int index);
  void *get_top();
  int get_chain_size(void);
  int chain_generate_ident(void);
  int test_print();
  int remove_all();
  int chain_empty();
  int disable_auto_remove();
  int enable_auto_remove();
};
#endif
