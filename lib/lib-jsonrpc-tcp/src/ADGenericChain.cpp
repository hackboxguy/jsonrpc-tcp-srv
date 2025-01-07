#include "ADGenericChain.hpp"
#include <iostream>
using namespace std;
int ADChainProducer::IDGenerator = 0;
ADGenericChain::ADGenericChain() {
  disable_autoremove = 0;
  chain_peak_size = 0;
  chain_size = 0;
  ident_generator = 0;
  base_chain.pNext = NULL;
  SEMA_INIT();
  init_flag = true;
}
ADGenericChain::~ADGenericChain() {
  SEMA_LOCK();
  remove_all();
  init_flag = false;
  SEMA_UNLOCK();
}
int ADGenericChain::chain_empty() {
  SEMA_LOCK();
  remove_all();
  ident_generator = 0;
  SEMA_UNLOCK();
  return 0;
}
int ADGenericChain::test_print(void) {
  cout << "This is ADChain" << endl;
  return 0;
}
void ADGenericChain::chain_lock() { SEMA_LOCK(); }
void ADGenericChain::chain_unlock() { SEMA_UNLOCK(); }
int ADGenericChain::chain_add(void *data) {
  struct chain_holder *pHolder = &base_chain;
  struct chain_holder *pNewHolder;
  pNewHolder = (struct chain_holder *)malloc(sizeof(struct chain_holder));
  if (pNewHolder == NULL)
    return -1;
  pNewHolder->pData = data;
  while (pHolder->pNext != NULL)
    pHolder = pHolder->pNext;
  pHolder->pNext = pNewHolder;
  pNewHolder->pPrev = pHolder;
  pNewHolder->pNext = NULL;
  chain_size++;
  if (chain_size > chain_peak_size)
    chain_peak_size = chain_size;
  return 0;
}
int ADGenericChain::chain_put(void *data) {
  int iRet = 0;
  SEMA_LOCK();
  if (init_flag == false) {
    SEMA_UNLOCK();
    return -1;
  }
  iRet = chain_add(data);
  SEMA_UNLOCK();
  return iRet;
}
void *ADGenericChain::chain_get_by_ident(int ident) {
  struct chain_holder *pHolder = base_chain.pNext;
  if (pHolder == NULL)
    return NULL;
  if (is_helper_attached() != 0)
    return NULL;
  while (pHolder != NULL) {
    if (identify_chain_element(pHolder->pData, ident) == 0)
      break;
    pHolder = pHolder->pNext;
  }
  if (pHolder == NULL)
    return NULL;
  else
    return pHolder->pData;
}
void *ADGenericChain::chain_remove_by_ident(int ident) {
  void *data = NULL;
  SEMA_LOCK();
  struct chain_holder *pHolder = base_chain.pNext;
  if (init_flag == false) {
    SEMA_UNLOCK();
    return NULL;
  }
  if (pHolder == NULL) {
    SEMA_UNLOCK();
    return NULL;
  }
  if (is_helper_attached() != 0) {
    SEMA_UNLOCK();
    return NULL;
  }
  while (pHolder != NULL) {
    if (identify_chain_element(pHolder->pData, ident) == 0)
      break;
    pHolder = pHolder->pNext;
  }
  if (pHolder == NULL) {
    SEMA_UNLOCK();
    return NULL;
  } else {
    data = pHolder->pData;
    if (pHolder->pNext != NULL) {
      pHolder->pPrev->pNext = pHolder->pNext;
      pHolder->pNext->pPrev = pHolder->pPrev;
    } else
      pHolder->pPrev->pNext = NULL;
    free(pHolder);
    pHolder = NULL;
    chain_size--;
  }
  SEMA_UNLOCK();
  return data;
}
void *ADGenericChain::chain_remove_by_double_ident(int ident1, int ident2) {
  void *data = NULL;
  SEMA_LOCK();
  struct chain_holder *pHolder = base_chain.pNext;
  if (init_flag == false) {
    SEMA_UNLOCK();
    return NULL;
  }
  if (pHolder == NULL) {
    SEMA_UNLOCK();
    return NULL;
  }
  if (is_helper_attached() != 0) {
    SEMA_UNLOCK();
    return NULL;
  }
  while (pHolder != NULL) {
    if (double_identify_chain_element(pHolder->pData, ident1, ident2) == 0)
      break;
    pHolder = pHolder->pNext;
  }
  if (pHolder == NULL) {
    SEMA_UNLOCK();
    return NULL;
  } else {
    data = pHolder->pData;
    if (pHolder->pNext != NULL) {
      pHolder->pPrev->pNext = pHolder->pNext;
      pHolder->pNext->pPrev = pHolder->pPrev;
    } else
      pHolder->pPrev->pNext = NULL;
    free(pHolder);
    pHolder = NULL;
    chain_size--;
  }
  SEMA_UNLOCK();
  return data;
}
void *ADGenericChain::chain_get_by_double_ident(int ident1, int ident2) {
  struct chain_holder *pHolder = base_chain.pNext;
  if (pHolder == NULL)
    return NULL;
  if (is_helper_attached() != 0)
    return NULL;
  while (pHolder != NULL) {
    if (double_identify_chain_element(pHolder->pData, ident1, ident2) == 0)
      break;
    pHolder = pHolder->pNext;
  }
  if (pHolder == NULL)
    return NULL;
  else
    return pHolder->pData;
}
void *ADGenericChain::chain_get_by_index(int index) {
  int i = 0;
  struct chain_holder *pHolder = base_chain.pNext;
  if (pHolder == NULL)
    return NULL;
  if (index >= chain_size)
    return NULL;
  while (i++ < index)
    pHolder = pHolder->pNext;
  if (pHolder == NULL)
    return NULL;
  else
    return pHolder->pData;
}
void *ADGenericChain::chain_get() {
  struct chain_holder *pHolder;
  void *data;
  SEMA_LOCK();
  if (init_flag == false) {
    SEMA_UNLOCK();
    return NULL;
  }
  pHolder = base_chain.pNext;
  if (pHolder == NULL) {
    SEMA_UNLOCK();
    return NULL;
  }
  data = pHolder->pData;
  chain_remove();
  SEMA_UNLOCK();
  return data;
}
void *ADGenericChain::get_top() {
  void *data;
  struct chain_holder *pHolder;
  if (init_flag == false) {
    return NULL;
  }
  pHolder = base_chain.pNext;
  if (pHolder == NULL) {
    return NULL;
  }
  data = pHolder->pData;
  return data;
}
int ADGenericChain::chain_remove(void) {
  struct chain_holder *pHolder = base_chain.pNext;
  if (pHolder == NULL)
    return -1;
  if (pHolder->pNext != NULL) {
    pHolder->pPrev->pNext = pHolder->pNext;
    pHolder->pNext->pPrev = pHolder->pPrev;
  } else
    pHolder->pPrev->pNext = NULL;
  free(pHolder);
  pHolder = NULL;
  chain_size--;
  return 0;
}
int ADGenericChain::get_chain_size(void) {
  int size;
  SEMA_LOCK();
  size = chain_size;
  SEMA_UNLOCK();
  return size;
}
int ADGenericChain::chain_generate_ident(void) {
  int ident;
  SEMA_LOCK();
  ident_generator++;
  ident = ident_generator;
  SEMA_UNLOCK();
  return ident;
}
int ADGenericChain::remove(void) {
  struct chain_holder *pHolder = base_chain.pNext;
  if (pHolder == NULL)
    return -1;
  if (pHolder->pNext != NULL) {
    pHolder->pPrev->pNext = pHolder->pNext;
    pHolder->pNext->pPrev = pHolder->pPrev;
  } else
    pHolder->pPrev->pNext = NULL;
  free(pHolder);
  pHolder = NULL;
  chain_size--;
  return 0;
}
int ADGenericChain::remove_all(void) {
  while (base_chain.pNext != NULL) {
    if (disable_autoremove == 0) {
      free_chain_element_data(base_chain.pNext->pData);
      free(base_chain.pNext->pData);
      base_chain.pNext->pData = NULL;
    }
    remove();
  }
  return 0;
}
int ADGenericChain::disable_auto_remove() {
  disable_autoremove = 1;
  return 0;
}
int ADGenericChain::enable_auto_remove() {
  disable_autoremove = 0;
  return 0;
}
