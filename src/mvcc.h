/*
 * Simple MVCC (Multi-Version Concurrency Control) for two varibles. A number
 * of threads created with initial version number and data variables repeat
 * UPDATE operation based on read-view at each moment.
 *
 * A version number is an unique number across the threads, and can be obtained
 * by using fetch-and-add counter. Two data variables, say A and B, are
 * long-typed integer whose total should be always a constant integer C.
 *
 * There is a list of (thread ID, version) pairs named 'global active thread
 * list'. This is the list of threads running UPDATE operation (described
 * later) with each version number of data variables which the thread is
 * working on. A read-view is a snapshot of global active thread list that
 * taken at the start of UPDATE operation.
 *
 * Before taking snapshot at the start of UPDATE operation in thread Ti, a new
 * version number v is obtained, and (Ti, v) is appended to global active
 * thread list. Obtaining v and taking snapshot (read-view) is done atomically.
 *
 * During UPDATE operation in Ti, another thread Tj is randomly-picked, and
 * data variables of Tj are read based on the read-view. 'Based on the
 * read-view' means that, if Tj is in the read-view, the version of data
 * variables to be read becomes the version associated with Tj in the
 * read-view. Otherwise, this becomes the latest version which is lower than
 * v.
 *
 * Once data variables A and B of Tj is read, value of A and B of Ti is updated
 * to A(Ti) + A(Tj) and B(Ti) - A(Tj). If verifying option is set in program
 * options, the summed value of A and B for every thread based on read-view is
 * checked if it is a constant (n_threads * C). Then, with the new value of A
 * and B of Ti, a new version is written. This is the end of UPDATE operation.
 *
 * After that, the pair with thread ID Ti should be removed from global active
 * thread list. This is also done atomatically.
 *
 * Author: Sanggyu Nam <pokeplus@gmail.com>
 */
#ifndef MULTICORE_MVCC_H_
#define MULTICORE_MVCC_H_

/* Type for program options. */
typedef struct {
  /* Number of threads. */
  int n_threads;
  /* Duration of running program, in seconds. */
  int duration;
  /* Whether invariant of variables based on read-view should be verified on
   * each UPDATE operation. If nonzero, the invariant should be verified. */
  int verify;
} program_options_t;

/* Runs MVCC according to program options, and writes UPDATE operation count
 * in each thread created while running MVCC.
 *
 * Pointer to program options object which is const opt and pointer to int data
 * n_updates should be given as input. n_updates should point to memory space
 * for a number of int data, and the number should be equal to opt->n_threads.
 * It is user's responsibility to free this memory space.
 *
 * After running MVCC, the memory space pointed by n_updates will be populated
 * with UPDATE operation count in each thread.
 *
 * Returns 0 if running was successful. */
int run_mvcc(const program_options_t* opt, int* n_updates);

#endif  /* MULTICORE_MVCC_H_ */
