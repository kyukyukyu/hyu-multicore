#include "trx.h"

namespace multicore {

lockmgr_t g_lockmgr;

int run_transaction(int thread_idx, trx_t** p_trx) {
  return 0;
}

int lockmgr_create(void) {
  int retval;
  // Number of buckets to be allocated.
  const unsigned int n_buckets = g_table_size;
  g_lockmgr.n_buckets = n_buckets;
  g_lockmgr.buckets = new lockmgr_t::locklist_t[n_buckets];
  for (unsigned int i = 0; i < n_buckets; ++i) {
    retval = list_init(&g_lockmgr.buckets[i]);
    if (retval) {
      return retval;
    }
  }
  return 0;
}

void lockmgr_free(void) {
  auto i = g_lockmgr.n_buckets - 1;
  while (i) {
    list_free(&g_lockmgr.buckets[i]);
    --i;
  }
  delete g_lockmgr.buckets;
}

int trx_init(int thread_idx, trx_t* trx) {
  trx->trx_id = __sync_add_and_fetch(&g_counter_trx, 1);
  trx->thread_idx = thread_idx;
  trx->trx_state = trx_t::IDLE;
  if (pthread_mutex_init(&trx->trx_mutex, NULL)) {
    return ERRCODE_TO_INT(ERR_TRX_MUTEX_INIT);
  }
  if (pthread_cond_init(&trx->trx_cond, NULL)) {
    return ERRCODE_TO_INT(ERR_TRX_COND_INIT);
  }
  trx->wait_lock = NULL;
  return 0;
}

void trx_free(trx_t* trx) {
  for (auto& lock : trx->trx_locks) {
    delete lock;
  }
  pthread_mutex_destroy(&trx->trx_mutex);
  pthread_cond_destroy(&trx->trx_cond);
}

} // namespace multicore
