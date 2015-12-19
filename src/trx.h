#ifndef MULTICORE_TRX_H_
#define MULTICORE_TRX_H_

extern "C" {
#include <pthread.h>
}

#define ERRCODE_TO_INT(x) (static_cast<int>((x)))

namespace multicore {

// Enum for error codes.
enum errcode_t {
  ERR_UNKNOWN_OPTION = 1,
  ERR_INVALID_TABLE_SIZE,
  ERR_INVALID_NUM_THREAD,
  ERR_INVALID_READ_NUM,
  ERR_INVALID_DURATION,
  ERR_CREATE_TABLES,
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
// Table A.
extern record_t* g_table_a;
// Table B.
extern record_t* g_table_b;

} // namespace multicore

#endif  // MULTICORE_TRX_H_
