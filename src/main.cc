#include <cstdio>

extern "C" {
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
}

#include "trx.h"

namespace multicore {

int g_table_size;
int g_num_thread;
int g_read_num;
int g_duration;
// Array of POSIX threads.
pthread_t* g_threads;
// Number of READ operations during the test.
unsigned long g_n_read = 0;
// Number of UPDATE operations during the test.
unsigned long g_n_update = 0;
// Number of aborted transactions during the test.
unsigned long g_n_aborted = 0;
// Global counter for transaction ID. The first transaction has ID 1.
unsigned long g_counter_trx = 0;

// Parses arguments passed to process into program options.
int parse_args(int argc, char* argv[]);
// Creates two tables which have g_table_size records each.
int create_tables(void);
// Thread routine which runs transactions endlessly.
void* thread_body(void*);
// Prints stats for the test.
int print_stats(void);

int main(int argc, char* argv[]) {
  if (parse_args(argc, argv)) {
    return static_cast<int>(ERR_ARGUMENTS);
  }
  if (create_tables()) {
    return static_cast<int>(ERR_CREATE_TABLES);
  }
  g_threads = new pthread_t[g_num_thread];
  for (int i = 0; i < g_num_thread; ++i) {
    pthread_create(&g_threads[i], NULL, thread_body, NULL);
  }
  // Zzz...
  sleep(g_duration);
  // Now I am awake! First, cancel threads.
  for (int i = 0; i < g_num_thread; ++i) {
    pthread_cancel(g_threads[i]);
  }
  // Wait for threads to complete.
  for (int i = 0; i < g_num_thread; ++i) {
    pthread_join(g_threads[i], NULL);
  }
  if (print_stats()) {
    return static_cast<int>(ERR_PRINT_STATS);
  }
  delete g_threads;
  return 0;
}

} // namespace multicore
