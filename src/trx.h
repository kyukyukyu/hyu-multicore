#ifndef MULTICORE_TRX_H_
#define MULTICORE_TRX_H_

#include <vector>

extern "C" {
#include <pthread.h>
}

#include "list.h"

#define ERRCODE_TO_INT(x) (static_cast<int>((x)))

namespace multicore {

// Type for transaction object.
struct trx_t;
// Type for lock object.
struct lock_t;

// Enum for error codes.
enum errcode_t {
  ERR_UNKNOWN_OPTION = 1,
  ERR_INVALID_TABLE_SIZE,
  ERR_INVALID_NUM_THREAD,
  ERR_INVALID_READ_NUM,
  ERR_INVALID_DURATION,
  ERR_CREATE_TABLES,
  ERR_TRX_MUTEX_INIT,
  ERR_TRX_COND_INIT,
  ERR_PRINT_STATS
};

// Record type.
struct record_t {
  // Record ID. The first record has ID 1.
  unsigned long id;
  // Value for this record.
  long value;
  // ID of the last transaction who has updated the value of this record.
  unsigned long last_updated_trx_id;
};
struct trx_t {
  // ID for this transaction.
  unsigned long trx_id;
  // Index of thread this transaction is running on.
  int thread_idx;
  // List of lock objects this transaction is holding.
  std::vector<lock_t*> trx_locks;
  // State of this transaction.
  enum state_t {
    RUNNING, WAITING, IDLE
  } trx_state;
  // Mutex for sleeping/waking-up this transaction.
  pthread_mutex_t trx_mutex;
  // Condition variable for sleeping/waking-up this transaction.
  pthread_cond_t trx_cond;
  // Lock object this transaction is waiting for. nullptr if this transaction
  // is waiting for no one.
  lock_t* wait_lock;
};
struct lock_t {
  // Table ID. 0 for A and 1 for B.
  unsigned long table_id;
  // Record ID.
  unsigned long record_id;
  // Lock mode.
  enum mode_t {
    SHARED, EXCLUSIVE
  } mode;
  // Lock state.
  enum state_t {
    WAITING, ACQUIRED, LOGICALLY_RELEASED
  } state;
  // Pointer to holder transaction.
  trx_t* trx;
};
// Type for lock manager. Internally, this works like hash table.
struct lockmgr_t {
  typedef list_t<lock_t*> locklist_t;
  // List of buckets. Each bucket has lock lists for records mixed.
  locklist_t* buckets;
  // Number of buckets.
  unsigned int n_buckets;
};

// The number of records in single table.
extern int g_table_size;
// The number of threads to be created.
extern int g_num_thread;
// The number of READ operations in single transaction.
extern int g_read_num;
// Duration for the test.
extern int g_duration;
// Number of READ operations during the test.
extern unsigned long g_n_read;
// Number of UPDATE operations during the test.
extern unsigned long g_n_update;
// Number of aborted transactions during the test.
extern unsigned long g_n_aborted;
// Global counter for transaction ID. The first transaction has ID 1.
extern unsigned long g_counter_trx;
// Data tables.
extern record_t* g_table[2];
// Global lock manager object.
extern lockmgr_t g_lockmgr;

// Creates a transaction object with given thread index, and runs a transaction
// for this.
int run_transaction(int thread_idx, trx_t** p_trx);
// Creates lock manager.
int lockmgr_create(void);
// Frees lock manager.
void lockmgr_free(void);
// Initializes a transaction object with given thread index.
int trx_init(int thread_idx, trx_t* trx);
// Frees a transaction object. This releases locks the transaction is holding.
void trx_free(trx_t* trx);

} // namespace multicore

#endif  // MULTICORE_TRX_H_
