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

void trx_free(trx_t* trx) {
}

} // namespace multicore
