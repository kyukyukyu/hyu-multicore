#include "mvcc.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define C 1024

/* Typedef for data variables. */
typedef long mvcc_data_t;
/* Typedef for version numbers. */
typedef unsigned int mvcc_vnum_t;
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
 * required for this function.
 *
 * Returns nonzero value if an error occured. */
static int add_version(const mvcc_data_t a, const mvcc_data_t b,
                       const mvcc_vnum_t vnum, const int thread_id);
/* Thread routine for MVCC. Repeats UPDATE operation forever. Arguments in
 * mvcc_args_t type should be given as input. Returns NULL. */
static void* mvcc_thread(void* args);

int run_mvcc(const program_options_t* opt, int* n_updates) {
  /* Loop variable. */
  int i;
  /* Pointer to memory space for threads. */
  pthread_t* threads;
  /* Pointer to memory space for the list of arguments to thread routine. */
  mvcc_args_t* argslist;
  /* Set initial version for each thread. */
  for (i = 0; i < opt->n_threads; ++i) {
    const mvcc_vnum_t vnum = get_new_vnum();
    const mvcc_data_t a = mrand48();
    const mvcc_data_t b = C - a;
    if (add_version(a, b, vnum, i)) {
      return 1;
    }
  }
  /* Create and run threads. */
  threads = (pthread_t*) calloc(opt->n_threads, sizeof(pthread_t));
  argslist = (mvcc_args_t*) calloc(opt->n_threads, sizeof(mvcc_args_t));
  if (NULL == threads || NULL == argslist) {
    fputs("Allocation of memory space for threads was not successful.\n",
          stderr);
    return 1;
  }
  for (i = 0; i < opt->n_threads; ++i) {
    mvcc_args_t* args = &argslist[i];
    args->thread_id = i;
    args->ptr_n_updates = &n_updates[i];
    pthread_create(&threads[i], NULL, mvcc_thread, (void*) args);
  }
  /* Fall asleep for duration. Created threads will keep running. */
  sleep(opt->duration);
  /* Cancel threads. */
  for (i = 0; i < opt->n_threads; ++i) {
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
