#include "ADGenericChain.hpp"
#include "adtest.hpp"
#include <pthread.h>

namespace {
struct Item {
  int ident;
  int *payload;
};
class Consumer : public ADChainConsumer {
public:
  int freed;
  Consumer() : freed(0) {}
  virtual int identify_chain_element(void *element, int ident,
                                     ADChainProducer *pObj) {
    return ((Item *)element)->ident == ident ? 0 : -1;
  }
  virtual int double_identify_chain_element(void *element, int ident1,
                                            int ident2, ADChainProducer *pObj) {
    return -1;
  }
  virtual int free_chain_element_data(void *element, ADChainProducer *pObj) {
    Item *it = (Item *)element;
    delete it->payload;
    it->payload = NULL;
    freed++;
    return 0;
  }
};
} // namespace

TEST_CASE("chain: put/get keeps FIFO order") {
  Consumer c;
  ADGenericChain chain;
  chain.attach_helper(&c);
  chain.set_element_deleter(&chain_delete_object<Item>);
  for (int i = 0; i < 100; i++) {
    Item *it = new Item;
    it->ident = i;
    it->payload = NULL;
    REQUIRE(chain.chain_put(it) == 0);
  }
  CHECK_EQ(chain.get_chain_size(), 100);
  for (int i = 0; i < 100; i++) {
    Item *it = (Item *)chain.chain_get();
    REQUIRE(it != NULL);
    CHECK_EQ(it->ident, i);
    delete it;
  }
  CHECK(chain.chain_get() == NULL);
}

TEST_CASE("chain: remove_by_ident removes only the match") {
  Consumer c;
  ADGenericChain chain;
  chain.attach_helper(&c);
  chain.set_element_deleter(&chain_delete_object<Item>);
  for (int i = 0; i < 10; i++) {
    Item *it = new Item;
    it->ident = i;
    it->payload = NULL;
    chain.chain_put(it);
  }
  Item *it = (Item *)chain.chain_remove_by_ident(5);
  REQUIRE(it != NULL);
  CHECK_EQ(it->ident, 5);
  delete it;
  CHECK(chain.chain_remove_by_ident(5) == NULL);
  CHECK_EQ(chain.get_chain_size(), 9);
}

// C3: remove_all() must call the consumer once per element and release the
// element with the matching deallocator (ASan flags new/free mismatches).
TEST_CASE("chain: remove_all frees every element with the right deleter") {
  Consumer c;
  {
    ADGenericChain chain;
    chain.attach_helper(&c);
    chain.set_element_deleter(&chain_delete_object<Item>);
    for (int i = 0; i < 50; i++) {
      Item *it = new Item;
      it->ident = i;
      it->payload = new int(i);
      chain.chain_put(it);
    }
    chain.chain_empty();
    CHECK_EQ(c.freed, 50);
    CHECK_EQ(chain.get_chain_size(), 0);
    for (int i = 0; i < 7; i++) {
      Item *it = new Item;
      it->ident = i;
      it->payload = new int(i);
      chain.chain_put(it);
    }
  } // destructor drains the remaining 7
  CHECK_EQ(c.freed, 57);
}

namespace {
struct ThreadArg {
  ADGenericChain *chain;
  int count;
  int got;
};
void *producer_thread(void *p) {
  ThreadArg *a = (ThreadArg *)p;
  for (int i = 0; i < a->count; i++) {
    Item *it = new Item;
    it->ident = i;
    it->payload = NULL;
    a->chain->chain_put(it);
  }
  return NULL;
}
void *consumer_thread(void *p) {
  ThreadArg *a = (ThreadArg *)p;
  while (a->got < a->count) {
    Item *it = (Item *)a->chain->chain_get();
    if (it != NULL) {
      delete it;
      a->got++;
    }
  }
  return NULL;
}
} // namespace

TEST_CASE("chain: concurrent put/get from 8 threads") {
  ADGenericChain chain;
  chain.set_element_deleter(&chain_delete_object<Item>);
  const int N = 5000;
  pthread_t prod[4], cons[4];
  ThreadArg pa[4], ca[4];
  for (int i = 0; i < 4; i++) {
    pa[i].chain = &chain;
    pa[i].count = N;
    pa[i].got = 0;
    ca[i] = pa[i];
    pthread_create(&prod[i], NULL, producer_thread, &pa[i]);
    pthread_create(&cons[i], NULL, consumer_thread, &ca[i]);
  }
  for (int i = 0; i < 4; i++) {
    pthread_join(prod[i], NULL);
    pthread_join(cons[i], NULL);
  }
  CHECK_EQ(chain.get_chain_size(), 0);
}

ADTEST_MAIN()
