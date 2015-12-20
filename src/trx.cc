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

lockmgr_t::locklist_t* lockmgr_bucket(unsigned long table_id,
    unsigned long record_id) {
  return &g_lockmgr.buckets[record_id - 1];
}

int lockmgr_acquire(unsigned long table_id, unsigned long record_id,
    trx_t* trx, lock_t::mode_t mode) {
  // TODO: lock hash table.
  lockmgr_t::locklist_t* bucket = lockmgr_bucket(table_id, record_id);
  bool conflicts = false;
  auto* curr = bucket->head;
  // Check if any conflict exists.
  if (lock_t::SHARED == mode) {
    while (curr) {
      auto* curr_lock = curr->value;
      if (!(table_id == curr_lock->table_id &&
            record_id == curr_lock->record_id)) {
        curr = curr->next;
        continue;
      }
      if (lock_t::EXCLUSIVE == curr_lock->mode) {
        conflicts = true;
        break;
      }
      curr = curr->next;
    }
  } else {  // EXCLUSIVE
    while (curr) {
      auto* curr_lock = curr->value;
      if (table_id == curr_lock->table_id &&
          record_id == curr_lock->record_id) {
        conflicts = true;
        break;
      }
      curr = curr->next;
    }
  }
  if (conflicts) {
    // curr must be the first conflicting lock with me at this moment.
    if (lockmgr_detect_deadlock(curr->value, trx)) {
      return 1;
    }
  }
  auto* new_lock = new lock_t;
  new_lock->table_id = table_id;
  new_lock->record_id = record_id;
  new_lock->mode = mode;
  new_lock->trx = trx;
  new_lock->state = conflicts ? lock_t::WAITING : lock_t::ACQUIRED;
  list_append(new_lock, bucket);
  if (conflicts) {
    pthread_mutex_lock(&trx->trx_mutex);
    // TODO: Unlock hash table.
    pthread_cond_wait(&trx->trx_cond, &trx->trx_mutex);
    // TODO: Lock hash table.
    pthread_mutex_unlock(&trx->trx_mutex);
  }
  // TODO: Unlock hash table.
  return 0;
}

void lockmgr_release(lock_t* lock) {
  const auto table_id = lock->table_id;
  const auto record_id = lock->record_id;
  const auto mode = lock->mode;
  // TODO: lock hash table.
  lockmgr_t::locklist_t* bucket = lockmgr_bucket(table_id, record_id);
  // True if I am the first one for given table ID and record ID. This should
  // be checked to decide whether I should wake up blocked transactions by me.
  bool first = true;
  auto* curr = bucket->head;
  // Pointer to node in lock list which has my lock as value.
  listnode_t<lock_t*>* mine;
  // Lock blocked by me. If there are more than one blocked lock, none of them
  // will be assigned to this variable. Instead, they will be waken up one by
  // one.
  lock_t* blocked = nullptr;
  auto* trx = lock->trx;
  // Find node which has lock object as value.
  while (curr) {
    auto* curr_lock = curr->value;
    if (table_id == curr_lock->table_id && record_id == curr_lock->record_id) {
      if (trx == curr_lock->trx) {
        mine = curr;
        break;
      }
      first = false;
    }
    curr = curr->next;
  }
  // Wake up, if any, the thread who wants to acquire X lock but is blocked by
  // me.
  if (first) {
    // This becomes true if there is any S lock to be waken up by me.
    bool wake_s = false;
    curr = mine;
    if (lock_t::SHARED == mode) {
      while (curr) {
        auto* curr_lock = curr->value;
        if (table_id == curr_lock->table_id &&
            record_id == curr_lock->record_id) {
          // There is a lock after me.
          if (lock_t::EXCLUSIVE == curr_lock->mode) {
            // The first one is exclusive.
            blocked = curr_lock;
          }
          // I don't care any lock after the first one: rest will be waken up by
          // other transaction.
          break;
        }
        curr = curr->next;
      }
    } else {  // EXCLUSIVE
      while (curr) {
        auto* curr_lock = curr->value;
        if (table_id == curr_lock->table_id &&
            record_id == curr_lock->record_id) {
          // There is lock after me.
          if (lock_t::EXCLUSIVE == curr_lock->mode) {
            if (!wake_s) {
              // The first lock after me is X lock: wake this up.
              blocked = curr_lock;
            }
            // I don't care any lock after the first one: rest will be waken up by
            // other transaction.
            break;
          }
          // The first lock after me is not X lock: wake S locks one by one.
          wake_s = true;
          lockmgr_wakeup(curr_lock);
        }
        curr = curr->next;
      }
    }
  }
  list_remove(mine, bucket);
  delete mine;
  if (nullptr != blocked) {
    // There is one blocked lock which should be waken up by me: wake this up.
    // If there were more than one blocked lock, they have been waken up
    // already.
    lockmgr_wakeup(blocked);
  }
  // TODO: unlock hash table.
}

bool dfs_for_deadlock(lock_t* lock, trx_t* trx, bool* visited) {
  visited[trx->thread_idx] = true;
  unsigned long table_id = lock->table_id;
  unsigned long record_id = lock->record_id;
  auto* bucket = lockmgr_bucket(table_id, record_id);
  auto* curr = bucket->head;
  lock_t* curr_lock;
  while (curr && (curr_lock = curr->value) != lock) {
    if (!(table_id == curr_lock->table_id &&
          record_id == curr_lock->record_id)) {
      // Skip locks for other records.
      continue;
    }
    trx_t* curr_holder = curr_lock->trx;
    if (trx == curr_holder) {
      // Deadlock detected!!
      return true;
    }
    if (trx_t::WAITING == curr_holder->trx_state &&
        !visited[curr_holder->thread_idx]) {
      if (dfs_for_deadlock(curr_lock, trx, visited)) {
        return true;
      }
    }
    curr = curr->next;
  }
  return false;
}

int lockmgr_detect_deadlock(lock_t* lock, trx_t* trx) {
  bool* visited = new bool[g_num_thread]();
  bool detected = dfs_for_deadlock(lock, trx, visited);
  delete[] visited;
  return detected ? 1 : 0;
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
