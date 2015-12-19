#include <cstdio>

extern "C" {
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
}

#include "trx.h"

#define ARG_ERROR(msg) \
    (std::fprintf(stderr, "Invalid argument: %s\n", (msg)))

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
  int retval;
  if (retval = parse_args(argc, argv)) {
    return retval;
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

int parse_args(int argc, char* argv[]) {
  // Return value of getopt_long().
  int c;
  while (1) {
    static struct option long_options[] = {
      {"table_size", optional_argument, NULL, 't'},
      {"num_thread", optional_argument, NULL, 'n'},
      {"read_num", optional_argument, NULL, 'r'},
      {"duration", optional_argument, NULL, 'd'},
      {0, 0, 0, 0}
    };
    // Get next option.
    c = getopt_long(argc, argv, "t::n::r::d::", long_options, NULL);
    if (c == -1) {
      // No more options to get.
      break;
    }
    switch (c) {
    case 't':
      // -t or --table_size
      std::sscanf(optarg, "%d", &g_table_size);
      break;

    case 'n':
      // -n or --num_thread
      std::sscanf(optarg, "%d", &g_num_thread);
      break;

    case 'r':
      // -r or --read_num
      std::sscanf(optarg, "%d", &g_read_num);
      break;

    case 'd':
      // -d or --duration
      std::sscanf(optarg, "%d", &g_duration);
      break;

    default:
      std::fprintf(stderr, "What?? '%c' ('%d')\n", c, c);
      return ERRCODE_TO_INT(ERR_UNKNOWN_OPTION);
    }
  }
  if (0 >= g_table_size) {
    ARG_ERROR("table_size should be greater than 0");
    return ERRCODE_TO_INT(ERR_INVALID_TABLE_SIZE);
  }
  if (0 >= g_num_thread) {
    ARG_ERROR("num_thread should be greater than 0");
    return ERRCODE_TO_INT(ERR_INVALID_NUM_THREAD);
  }
  if (0 > g_read_num || 10 < g_read_num) {
    ARG_ERROR("read_num should be in [0, 10]");
    return ERRCODE_TO_INT(ERR_INVALID_READ_NUM);
  }
  if (0 >= g_duration) {
    ARG_ERROR("duration should be greater than 0");
    return ERRCODE_TO_INT(ERR_INVALID_DURATION);
  }
  return 0;
}

} // namespace multicore
