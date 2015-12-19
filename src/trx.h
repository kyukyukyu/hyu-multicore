#ifndef MULTICORE_TRX_H_
#define MULTICORE_TRX_H_

extern "C" {
#include <pthread.h>
}

namespace multicore {

// Enum for error codes.
enum errcode_t {
  ERR_ARGUMENTS = 1,
  ERR_CREATE_TABLES,
  ERR_PRINT_STATS
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

} // namespace multicore

#endif  // MULTICORE_TRX_H_
