#include "mvcc.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "linked_list.h"
#include "lock.h"

#define C 1024
#define THREAD_ERROR(thread_id, msg) \
    (fprintf(stderr, "Thread #%d error: %s\n", (thread_id), (msg)))

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
/* Typedef for elements of global active thread list. */
typedef struct {
  /* Thread ID. */
  int thread_id;
  /* Version number of data variables that the thread is working on. */
  mvcc_vnum_t vnum;
} mvcc_tvpair_t;

/* Returns a new, unique version number using fetch-and-add counter. It is
 * caller's responsibility to gain the lock for the counter. */
static mvcc_vnum_t get_new_vnum();
/* Adds a pair of thread ID and version number to global active thread list.
 * Returns nonzero value if adding was not successful. */
static int set_active(int thread_id, mvcc_vnum_t vnum);
/* Removes a pair with given thread ID from global active thread list. */
static void set_inactive(int thread_id);
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
/* Copies global active thread list to read-view. Pointer to memory space for
 * read-view should be given as input. Returns the number of pairs in read-view
 * on success, or negative integer on failure. */
static int take_read_view(mvcc_tvpair_t* read_view);
/* Reads data variables of arbitrary thread based on read-view. Pointer to
 * memory space for read-view, number of pairs in the read-view, ID of thread
 * whose data variable will be read, and version number of data variables in
 * current thread that will be updated should be given as input. Returns
 * pointer to memory space for version of data variables.*/
static mvcc_version_t* read_data(
    mvcc_tvpair_t* read_view,
    int n_tv_pairs,
    int tid_j,
    mvcc_vnum_t vnum);
/* Verifies constant invariant of data variables based on read-view. Pointer to
 * memory space for read-view, number of pairs in the read-view, and version
 * number of data variables in current thread that will be updated should be
 * given as input. Returns nonzero value if constant invariant is violated. */
static int verify_invariant(
    mvcc_tvpair_t* read_view,
    int n_tv_pairs,
    mvcc_vnum_t vnum);
/* Cleanup handler for threads. Frees memory spaces for read-view. Pointer to
 * the space should be given as input. */
static void mvcc_thread_cleanup(void* read_view);
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
static volatile mvcc_vnum_t g_version_counter = 0;
/* Pointer to memory space for histories of thread. */
static list_t* g_histories;
/* Global active thread list. 'atl' stands for 'Active Thread List'. */
static list_t g_atl;
/* Pointer to memory space for lock on global active thread list. */
static lock_t g_lock_atl;
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
  g_histories = (list_t*) calloc(g_n_threads, sizeof(list_t));
  if (NULL == g_histories) {
    fputs("failed to allocate memory space for histories\n", stderr);
    return 1;
  }
  /* Set initial version for each thread. */
  for (i = 0; i < g_n_threads; ++i) {
    const mvcc_vnum_t vnum = get_new_vnum();
    const mvcc_data_t a = rand() % 1024;
    const mvcc_data_t b = C - a;
    list_init(&g_histories[i]);
    if (add_version(a, b, vnum, i)) {
      return 1;
    }
  }
  /* Allocation of memory space for data used in threads. */
  threads = (pthread_t*) calloc(g_n_threads, sizeof(pthread_t));
  argslist = (mvcc_args_t*) calloc(g_n_threads, sizeof(mvcc_args_t));
  if (NULL == threads || NULL == argslist) {
    fputs("Allocation of memory space for threads was not successful.\n",
          stderr);
    return 1;
  }
  /* Initialize global active thread list and a lock for this. */
  list_init(&g_atl);
  if (lock_init(&g_lock_atl, g_n_threads)) {
    fputs("Initialization of lock was not successful.\n", stderr);
    return 1;
  }
  /* Create and run threads. */
  for (i = 0; i < g_n_threads; ++i) {
    mvcc_args_t* args = &argslist[i];
    args->thread_id = i;
    args->ptr_n_updates = &update_counts[i];
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
  /* Destroy lock for global active thread list. */
  lock_destroy(&g_lock_atl);
  /* Free memory space for threads. */
  free(threads);
  free(argslist);
  return 0;
}

mvcc_vnum_t get_new_vnum() {
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
  if (NULL == version) {
    fprintf(
        stderr,
        "failed to add version: a: %ld, b: %ld, vnum: %u, tid: %d\n",
        a, b, vnum, thread_id);
    return 1;
  }
  version->a = a;
  version->b = b;
  version->vnum = vnum;
  return list_insert((void*) version, ptr_list, 0);
}

void* mvcc_thread(void* args_void) {
  /* Pointer to arguments, type-casted. */
  const mvcc_args_t* args = (mvcc_args_t*) args_void;
  /* Thread ID. */
  const int thread_id = args->thread_id;
  /* Data variables that this thread is working on. */
  mvcc_data_t a, b;
  /* Version number of data variables that this thread is working on. */
  mvcc_vnum_t vnum;
  /* Pointer to history list for this thread. */
  list_t* history = &g_histories[thread_id];
  /* Pointer to latest (which is initial at the first time) version of data
   * variables for this thread. */
  mvcc_version_t* latest_ver;
  /* Read-view. What makes MVCC possible. */
  mvcc_tvpair_t* read_view;
  /* The number of pairs in read-view. */
  int n_rv_pairs;

  /* Load the initial version of data variables. */
  latest_ver = (mvcc_version_t*) list_at(history, 0);
  a = latest_ver->a;
  b = latest_ver->b;
  vnum = latest_ver->vnum;
  /* Allocate memory space for read-view. Will be freed in thread cleanup
   * handler. */
  read_view = (mvcc_tvpair_t*) calloc(g_n_threads, sizeof(mvcc_tvpair_t));
  if (NULL == read_view) {
    /* Allocation was not successful. */
    THREAD_ERROR(
        thread_id,
        "memory space allocation for read-view was not successful");
    pthread_exit(NULL);
    return NULL;
  }
  /* Push cleanup handler for this thread. */
  pthread_cleanup_push(mvcc_thread_cleanup, (void*) read_view);

  /* To infinity, and beyond! */
  while (1) {
    /* ID of random-picked thread Tj. This should be other than ID of this
     * thread. */
    int tid_j;
    /* Version of data variables of thread Tj which is determined based on
     * read-view. */
    mvcc_version_t* data_j;
    /* Atomic guard starts ================================================= */
    lock_on(&g_lock_atl, thread_id);
    vnum = get_new_vnum();
    if (set_active(thread_id, vnum)) {
      /* This should not happen, but setting this thread active was not
       * successful. */
      THREAD_ERROR(thread_id, "setting thread active was not successful");
      goto terminate_thread;
    }
    /* Memory barrier is required here since setting this thread active must
     * be done before taking read-view. */
    __sync_synchronize();
    n_rv_pairs = take_read_view(read_view);
    if (n_rv_pairs <= 0) {
      /* The number of pairs in read-view cannot be <= 0 since this thread is
       * set active. Something was wrong on taking read-view. */
      THREAD_ERROR(thread_id, "taking read-view was not successful");
      goto terminate_thread;
    }
    lock_off(&g_lock_atl, thread_id);
    /* Atomic guard ends =================================================== */

    /* UPDATE operation starts ============================================= */
    /* Random-pick thread ID other than mine. Actually, rand() is not
     * thread-safe. But... that's not important for now. */
    tid_j = (thread_id + 1 + rand() % (g_n_threads - 1)) % g_n_threads;
    /* Read data variables of proper version based on read-view. */
    data_j = read_data(read_view, n_rv_pairs, tid_j, vnum);
    if (NULL == data_j) {
      /* Invalid data variables. */
      THREAD_ERROR(
          thread_id,
          "read invalid data variables from another thread.");
      goto terminate_thread;
    }
    a += data_j->a;
    b -= data_j->a;
    if (g_verify && verify_invariant(read_view, n_rv_pairs, vnum)) {
      /* --verify is on and constant invariant is violated. */
      THREAD_ERROR(thread_id, "constant invariant violation");
      goto terminate_thread;
    }
    add_version(a, b, vnum, thread_id);
    /* UPDATE operation ends =============================================== */

    /* Atomic guard starts ================================================= */
    lock_on(&g_lock_atl, thread_id);
    set_inactive(thread_id);
    lock_off(&g_lock_atl, thread_id);
    /* Atomic guard ends =================================================== */

    /* Increment UPDATE operation count for this thread. */
    ++(*args->ptr_n_updates);
  }
terminate_thread:
  pthread_cleanup_pop(1);
  pthread_exit(NULL);
  return NULL;
}

void mvcc_thread_cleanup(void* read_view) {
  free(read_view);
}

int set_active(int thread_id, mvcc_vnum_t vnum) {
  /* Pointer to memory space for pair of thread ID and version number. This
   * will be freed in set_inactive(). */
  mvcc_tvpair_t* ptr_tvpair = (mvcc_tvpair_t*) malloc(sizeof(mvcc_tvpair_t));
  if (NULL == ptr_tvpair) {
    THREAD_ERROR(thread_id, "failed to create (thread_id, vnum) pair");
    return 1;
  }
  ptr_tvpair->thread_id = thread_id;
  ptr_tvpair->vnum = vnum;
  return list_insert(ptr_tvpair, &g_atl, 0);
}

int crit_set_inactive(void* ptr_tvpair, void* ptr_thread_id) {
  return ((mvcc_tvpair_t*) ptr_tvpair)->thread_id == *((int*) ptr_thread_id);
}

void set_inactive(int thread_id) {
  /* Criteria closure for deleting a (tid, vnum) pair from global active thread
   * list. */
  crit_closure_t criteria = {crit_set_inactive, &thread_id};
  /* Pointer to memory space for (tid, vnum) pair which is removed from global
   * active thread list. */
  mvcc_tvpair_t* ptr_tvpair = list_delete_first(&g_atl, &criteria);
  free(ptr_tvpair);
}

int take_read_view(mvcc_tvpair_t* read_view) {
  /* Pointer to a node in global active thread list. */
  list_node_t* ptr_node;
  /* Index in read-view. */
  int i;
  ptr_node = g_atl.head;
  i = 0;
  while (ptr_node) {
    read_view[i] = *((mvcc_tvpair_t*) ptr_node->elem);
    ptr_node = ptr_node->next;
    ++i;
  }
  return i;
}

mvcc_version_t* read_data(
    mvcc_tvpair_t* read_view,
    int n_tv_pairs,
    int tid_j,
    mvcc_vnum_t vnum) {
  /* Boundary for version number. Data variables with version number lower than
   * this number should be read. */
  mvcc_vnum_t vnum_boundary = vnum;
  /* Loop variable. */
  int i;
  /* Pointer to a node in history for thread Tj. */
  list_node_t* ptr_node;
  for (i = 0; i < n_tv_pairs; ++i) {
    /* Pointer to (tid, vnum) pair at this index. */
    mvcc_tvpair_t* ptr_tvpair = &read_view[i];
    if (tid_j == ptr_tvpair->thread_id) {
      /* Thread Tj is active. */
      vnum_boundary = ptr_tvpair->vnum;
      break;
    }
  }
  ptr_node = g_histories[tid_j].head;
  while (ptr_node) {
    /* Pointer to version of data variables. */
    mvcc_version_t* ptr_version = (mvcc_version_t*) ptr_node->elem;
    if (vnum_boundary > ptr_version->vnum) {
      /* Found proper version. */
      return ptr_version;
    }
  }
  /* Uh, oh... */
  fprintf(
      stderr,
      "cannot read data older than version #%u from thread #%d\n",
      vnum,
      tid_j);
  return NULL;
}

int verify_invariant(
    mvcc_tvpair_t* read_view,
    int n_tv_pairs,
    mvcc_vnum_t vnum) {
  /* Thread ID. */
  int tid;
  for (tid = 0; tid < g_n_threads; ++tid) {
    /* Pointer to version of data variables from thread #tid. */
    mvcc_version_t* ptr_version;
    ptr_version = read_data(read_view, n_tv_pairs, tid, vnum);
    if (NULL == ptr_version) {
      return 1;
    }
    if (C != ptr_version->a + ptr_version->b) {
      /* Constant invariant violated. */
      fprintf(
          stderr,
          "constant invariant violated - tid: %d, vnum: %u, a: %ld, b: %ld\n",
          tid,
          ptr_version->vnum,
          ptr_version->a,
          ptr_version->b);
      return 1;
    }
  }
  return 0;
}

void catch_alarm(int sig) {
  g_run_main_loop = 0;
}
