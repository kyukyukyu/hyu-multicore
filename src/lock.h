/*
 * Lock implementation that guarantees mutual exclusion for critical sections.
 * This is based on Lamport's Bakery algorithm.
 *
 * To use lock, each of threads should have own ID which is zero-based. If n
 * threads are used, thread ID should be >= 0 && < n. A lock should be
 * initialized with the number of threads before the lock is used. On turning
 * the lock on and off, thread ID should be given as input. Before program
 * termination, a lock should be destroyed so that memory leaks by the lock can
 * be protected.
 *
 * Author: Sanggyu Nam <pokeplus@gmail.com>
 */
#ifndef MULTICORE_LOCK_H_
#define MULTICORE_LOCK_H_

/* Typedef for internal data types. */
typedef unsigned char lock_choosing_t;
typedef unsigned long lock_label_t;
/* Typedef for locks. */
typedef struct {
  /* Number of threads being used. */
  int n_threads;
  /* Array of choosing flag for each of threads. Nonzero value means that
   * corresponding thread is choosing its label. */
  lock_choosing_t* choosing;
  /* Array of label for each of threads. It is possible that more than two
   * threads have same label at the same time. However, since thread ID is also
   * compared on waiting for other threads, it is just OK. */
  lock_label_t* label;
} lock_t;

/* Initializes a lock. Pointer to memory space for lock and the number of
 * threads being used should be given as input. Returns nonzero value if
 * initialization was not successful. */
int lock_init(lock_t* ptr_lock, int n_threads);
/* Turns on a lock. Pointer to memory space for lock and thread ID should be
 * given as input. Returns when lock is acquired. */
void lock_on(lock_t* ptr_lock, int thread_id);
/* Turns off a lock. Pointer to memory space for lock and thread ID should be
 * given as input. */
void lock_off(lock_t* ptr_lock, int thread_id);
/* Destroys a lock. If memory space for the lock is dynamically allocated, it
 * is caller's responsibility to free the space. Pointer to memory space for
 * the lock should be given as input. */
void lock_destroy(lock_t* ptr_lock);

#endif    /* MULTICORE_LOCK_H_ */
