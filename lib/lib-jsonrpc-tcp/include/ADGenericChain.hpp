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
// Element ownership rules (see review finding C3):
// - remove_all()/chain_empty()/~ADGenericChain() first call the consumer's
//   free_chain_element_data(element) so it can release what the element
//   points to (buffers, json objects, ...). The consumer must NOT release the
//   element itself there.
// - The element itself is then released by the chain's element deleter. The
//   default deleter is free() for backward compatibility with elements
//   allocated by malloc(). Elements allocated with new (OBJECT_MEM_NEW) must
//   register a matching deleter, e.g.
//     chain.set_element_deleter(&chain_delete_object<MyType>);
//   set_element_deleter(NULL) leaves the element to the consumer.
// - disable_auto_remove() skips both steps (elements are owned elsewhere).
// Locking rules (review finding M1):
// - chain_put/chain_get/chain_remove_*/get_chain_size/chain_generate_ident
//   take the chain mutex internally.
// - chain_get_by_ident/chain_get_by_double_ident/chain_get_by_index/get_top
//   do NOT lock. Callers must hold chain_lock() and stop using the returned
//   pointer after chain_unlock().
// - The mutex is not recursive: never call a locking method between
//   chain_lock() and chain_unlock() on the same chain.
typedef void (*chain_element_deleter)(void *element);
template <typename T> void chain_delete_object(void *element) {
  delete static_cast<T *>(element);
}
inline void chain_free_element(void *element) { free(element); }
struct chain_holder {
  void *pData;
  struct chain_holder *pPrev;
  struct chain_holder *pNext;
};
class ADGenericChain : public ADChainProducer {
  int disable_autoremove;
  chain_element_deleter element_deleter;
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
  int set_element_deleter(chain_element_deleter deleter);
};
#endif
