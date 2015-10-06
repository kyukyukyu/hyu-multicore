#include "mvcc.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "linked_list.h"

#define C 1024

/* Typedef for data variables. */
typedef long mvcc_data_t;
/* Typedef for version numbers. */
typedef unsigned int mvcc_vnum_t;
/* Typedef for version entity. */
typedef struct {
  /* Data variable A. */
  mvcc_data_t a;
  /* Data variable B. */
  mvcc_data_t b;
  /* Version number. */
  mvcc_vnum_t vnum;
} mvcc_version_t;
/* Typedef for arguments to thread routine. */
typedef struct {
  /* Thread ID. */
  int thread_id;
  /* Pointer to memory space where UPDATE operation count will be stored. */
  int* ptr_n_updates;
} mvcc_args_t;

/* Returns a new, unique version number using fetch-and-add counter. It is
 * caller's responsibility to gain the lock for the counter. */
static unsigned int get_new_vnum();
/* Adds a new version of data variables for a thread. The value of two data
 * variables a and b, version number, and thread ID should be given as input.
 * Since only the owner thread can update its history, mutual exclusion is not
 * required for this function. It is caller's responsibility to allocate memory
 * space for pointer to head node for each of linked lists.
 *
 * Returns nonzero value if an error occured. */
static int add_version(const mvcc_data_t a, const mvcc_data_t b,
                       const mvcc_vnum_t vnum, const int thread_id);
/* Thread routine for MVCC. Repeats UPDATE operation forever. Arguments in
 * mvcc_args_t type should be given as input. Returns NULL. */
static void* mvcc_thread(void* args);
/* Handler for signal SIGALRM. Sets global flag g_run_main_loop to 0 so that
 * the loop started after creating threads in the main thread is terminated. */
static void catch_alarm(int sig);

/* Number of threads. Initialized in run_mvcc(). */
static int g_n_threads;
/* Whether invariant of variables based on read-view should be verified on each
 * UPDATE operation. If nonzero, the invariant should be verified. Initialized
 * in run_mvcc(). */
static int g_verify;
/* Global version counter variable. */
static mvcc_vnum_t g_version_counter = 0;
/* Pointer to memory space for histories of thread. */
static list_t* g_histories;
/* Flag for the loop in main thread that waits until duration is over while
 * checking the amount of histories and running garbage collection when
 * needed. */
static volatile sig_atomic_t g_run_main_loop = 1;

int run_mvcc(const program_options_t* opt, int* update_counts) {
  /* Loop variable. */
  int i;
  /* Pointer to memory space for threads. */
  pthread_t* threads;
  /* Pointer to memory space for the list of arguments to thread routine. */
  mvcc_args_t* argslist;
  /* Initialize global variables for program options. */
  g_n_threads = opt->n_threads;
  g_verify = opt->verify;
  /* Set initial version for each thread. */
  for (i = 0; i < g_n_threads; ++i) {
    const mvcc_vnum_t vnum = get_new_vnum();
    const mvcc_data_t a = mrand48();
    const mvcc_data_t b = C - a;
    if (add_version(a, b, vnum, i)) {
      return 1;
    }
  }
  /* Create and run threads. */
  threads = (pthread_t*) calloc(g_n_threads, sizeof(pthread_t));
  argslist = (mvcc_args_t*) calloc(g_n_threads, sizeof(mvcc_args_t));
  g_histories = (list_t*) calloc(g_n_threads, sizeof(list_t));
  if (NULL == threads || NULL == argslist || NULL == g_histories) {
    fputs("Allocation of memory space for threads was not successful.\n",
          stderr);
    return 1;
  }
  for (i = 0; i < g_n_threads; ++i) {
    mvcc_args_t* args = &argslist[i];
    args->thread_id = i;
    args->ptr_n_updates = &update_counts[i];
    list_init(&g_histories[i]);
    pthread_create(&threads[i], NULL, mvcc_thread, (void*) args);
  }
  /* Install handler for signal SIGALRM. This signal is raised when duration
   * is over since alarm() is called. */
  signal(SIGALRM, catch_alarm);
  /* Set an alarm that rings after duration. 'Ringing' means sending signal
   * SIGALRM to this process so that global flag g_run_main_loop is set to
   * 0. */
  alarm(opt->duration);
  /* Waits until the alarm rings while doing something. */
  while (g_run_main_loop) {
  }
  /* Cancel threads. */
  for (i = 0; i < g_n_threads; ++i) {
    if (pthread_cancel(threads[i])) {
      fprintf(stderr, "Could not cancel thread %d. (%02lx)\n",
              i, threads[i]);
    }
  }
  /* Free memory space for threads. */
  free(threads);
  free(argslist);
  return 0;
}

unsigned int get_new_vnum() {
  return g_version_counter++;
}

int add_version(const mvcc_data_t a, const mvcc_data_t b,
                const mvcc_vnum_t vnum, const int thread_id) {
  /* Pointer to the linked list for version history of the thread. */
  list_t* ptr_list = &g_histories[thread_id];
  /* Pointer to memory space for new version entity to be added. The memory
   * space will be freed by either garbage collector or teardown routine of
   * run_mvcc(). */
  mvcc_version_t* version = (mvcc_version_t*) malloc(sizeof(mvcc_version_t));
  version->a = a;
  version->b = b;
  version->vnum = vnum;
  list_insert((void*) version, ptr_list, 0);
}

void catch_alarm(int sig) {
  g_run_main_loop = 0;
}
