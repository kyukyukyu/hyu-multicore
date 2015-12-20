#include <cstdio>
#include <cstdlib>
#include <ctime>

extern "C" {
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
}

#include "trx.h"

#define ARG_ERROR(msg) \
    (std::fprintf(stderr, "Invalid argument: %s\n", (msg)))
#define COMPUTE_RATE(count, duration) \
    (static_cast<double>(count) / static_cast<double>(duration))

namespace multicore {
  // Main function for this program.
  int main(int argc, char* argv[]);
}

int main(int argc, char* argv[]) {
  return multicore::main(argc, argv);
}

namespace multicore {

int g_table_size;
int g_num_thread;
int g_read_num;
int g_duration;
record_t* g_table_a;
record_t* g_table_b;
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
int table_create(void);
// Thread routine which runs transactions endlessly.
void* thread_body(void* t_idx);
// Cleanup routine for a thread that frees the transaction object on which the
// thread was working.
void thread_cleanup(void* trx);
// Prints stats for the test.
int print_stats(void);
// Frees memory for two tables.
inline void table_free(void);

int main(int argc, char* argv[]) {
  int retval = 0;
  // Thread arguments.
  int* targs;
  // Set random seed.
  std::srand(std::time(NULL));
  if (retval = parse_args(argc, argv)) {
    return retval;
  }
  if (retval = table_create()) {
    return retval;
  }
  if (retval = lockmgr_create()) {
    return retval;
  }
  g_threads = new pthread_t[g_num_thread];
  targs = new int[g_num_thread];
  for (int i = 0; i < g_num_thread; ++i) {
    pthread_create(&g_threads[i], NULL, thread_body,
        static_cast<void*>(&targs[i]));
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
  if (retval = print_stats()) {
    return retval;
  }
  delete g_threads;
  delete targs;
  table_free();
  lockmgr_free();
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

int table_create(void) {
  g_table_a = new record_t[g_table_size];
  g_table_b = new record_t[g_table_size];
  for (int i = 0; i < g_table_size; ++i) {
    auto& record_a = g_table_a[i];
    auto& record_b = g_table_b[i];
    record_a.id = i + 1;
    record_a.value = (std::rand() % (100000 - 10000)) + 10000;
    record_a.last_updated_trx_id = 0;
    record_b.id = i + 1;
    record_b.value = (std::rand() % (100000 - 10000)) + 10000;
    record_b.last_updated_trx_id = 0;
  }
  return 0;
}

void* thread_body(void* t_idx) {
  // Pointer to transaction object.
  trx_t* trx = nullptr;
  // Register cleanup routine for this thread. Cleanup routine should free
  // transaction object on which this thread is working.
  pthread_cleanup_push(thread_cleanup, static_cast<void*>(trx));
  // Run endless test.
  while (1) {
    if (run_transaction(*static_cast<int*>(t_idx), &trx)) {
      // Something went wrong on running transaction. This does not mean that
      // the transaction is aborted.
      pthread_exit(NULL);
    }
  }
  pthread_cleanup_pop(0);
  return NULL;
}

void thread_cleanup(void* trx) {
  // Type-casted trx.
  auto _trx = static_cast<trx_t*>(trx);
  if (nullptr != _trx) {
    trx_free(_trx);
    delete _trx;
  }
}

int print_stats(void) {
  if (0 > std::printf("READ throughput: %lu READS and %lf READS/sec\n",
        g_n_read, COMPUTE_RATE(g_n_read, g_duration)) ||
      0 > std::printf("UPDATE throughput: %lu UPDATES and %lf UPDATES/sec\n",
        g_n_update, COMPUTE_RATE(g_n_update, g_duration)) ||
      0 > std::printf("Transaction throughput: %lu trx and %lf trx/sec\n",
        g_counter_trx - 1, COMPUTE_RATE(g_counter_trx - 1, g_duration)) ||
      0 > std::printf("Aborted transactions: %lu aborts and %lf aborts/sec\n",
        g_n_aborted, COMPUTE_RATE(g_n_aborted, g_duration))) {
    return ERRCODE_TO_INT(ERR_PRINT_STATS);
  }
  return 0;
}

void table_free(void) {
  delete g_table_a;
  delete g_table_b;
}

} // namespace multicore
