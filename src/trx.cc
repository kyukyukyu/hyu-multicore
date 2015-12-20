#include <cstdlib>

#include "trx.h"

#define RECORD(table_id, record_id) (g_table[(table_id)][(record_id) - 1])

namespace multicore {

lockmgr_t g_lockmgr;

int run_transaction(int thread_idx, trx_t** p_trx) {
  int retval = 0;
  trx_t* trx;
  // BEGIN with creating and initializing transaction object.
  trx = *p_trx = new trx_t;
  trx_init(thread_idx, trx);
  // Pick random number in [1, g_table_size - 9] for the first record index.
  unsigned long k = 1 + (std::rand() % (g_table_size - 9));
  // Pick either table A or table B.
  unsigned long table_id = std::rand() % 2;
  // Summation of results from READ operations.
  long sum = 0;
  for (unsigned long i = k; i < k + g_read_num; ++i) {
    // Value returned from READ operation.
    long val;
    if (db_read(table_id, i, trx, &val)) {
      // Deadlock detected!!
      retval = trx_abort(trx);
      goto end_of_trx;
    }
  }
  for (unsigned long i = k + g_read_num; i < k + 10; ++i) {
    if (db_update(i, trx)) {
      // Deadlock detected!!
      retval = trx_abort(trx);
      goto end_of_trx;
    }
  }
  retval = trx_commit(trx);
end_of_trx:
  // TODO: Move freeing of transaction obj. to inside of deadlock detection.
  trx_free(*p_trx);
  delete *p_trx;
  return retval;
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

int trx_commit(trx_t* trx) {
  for (auto& lock : trx->trx_locks) {
    lockmgr_release(lock);
  }
  trx->trx_state = trx_t::IDLE;
  return 0;
}

int trx_abort(trx_t* trx) {
  return trx_commit(trx);
}

void trx_free(trx_t* trx) {
  for (auto& lock : trx->trx_locks) {
    delete lock;
  }
  pthread_mutex_destroy(&trx->trx_mutex);
  pthread_cond_destroy(&trx->trx_cond);
}

int db_read(unsigned long table_id, unsigned long record_id, trx_t* trx,
    long* p_val) {
  if (lockmgr_acquire(table_id, record_id, trx, lock_t::SHARED)) {
    // Deadlock detected!!
    return 1;
  }
  *p_val = RECORD(table_id, record_id).value;
  return 0;
}

int db_update(unsigned long record_id, trx_t* trx) {
  if (lockmgr_acquire(0, record_id, trx, lock_t::EXCLUSIVE) ||
      lockmgr_acquire(1, record_id, trx, lock_t::EXCLUSIVE)) {
    // Deadlock detected!!
    return 1;
  }
  auto* record_a = &RECORD(0, record_id);
  auto* record_b = &RECORD(1, record_id);
  // Head or tail.
  int choice = std::rand() % 2;
  if (choice) {
    record_a->value -= 10;
    record_b->value += 10;
  } else {
    record_a->value += 10;
    record_b->value -= 10;
  }
  record_a->last_updated_trx_id = record_b->last_updated_trx_id = trx->trx_id;
  return 0;
}

} // namespace multicore
