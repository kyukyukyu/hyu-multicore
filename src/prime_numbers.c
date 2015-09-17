#include "taskqueue.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/* Type alias for marks. */
typedef unsigned char mark_t;
/* Type definition for arguments to be passed to sieve_mark_routine(). */
typedef struct {
  /* Index of array item which corresponds to the number whose multiples will
   * be marked as non-prime. */
  size_t i;
  /* Pointer to the head of mark array. */
  mark_t* marks;
  /* Length of array which marks points to. */
  size_t n_marks;
  /* The exclusive upper bound of searching prime numbers. */
  unsigned long b;
} markarg_t;
/*
 * Allocates space for the array of marks which is used for seqs in
 * find_prime_numbers(), and returns the length of array. Pointer to seqs
 * should be provided.
 */
size_t alloc_marks(mark_t** marks);
/*
 * For every odd number smaller than sqrt(b), marks the multiples of the number
 * non-prime. In multi-threaded fashion.
 */
void sieve_mark_iter(const size_t n_marks, mark_t* const marks);
/*
 * Thread routine which marks multiples of an odd-number which is not marked
 * as non-prime yet.
 */
void* sieve_mark_routine(void* arg);
/*
 * Filter the result of sieve_mark_iter() to count the number of prime numbers
 * between a and b, and returns the number of counted prime numbers. If verbose
 * is nonzero value, counted prime numbers are printed to stdout, one number in
 * one line.
 */
size_t sieve_filter(const mark_t* marks, const size_t n_marks);

/* Flag for verbose finding. If nonzero, found prime numbers will be printed
 * to stdout, one number in one line. */
static int verbose;
/* Lower bound for finding prime numbers. */
static unsigned long a;
/* Upper bound for finding prime numbers. */
static unsigned long b;
/* The number of threads to be used. */
static int n_threads;

size_t find_prime_numbers(const unsigned long _a,
                          const unsigned long _b,
                          const int _n_threads,
                          const int _verbose) {
  /* The number of prime numbers between a and b. */
  size_t n_prime = 0;
  /*
   * Pointer to the array of marks. Each mark represents if an odd number is
   * prime or not. The first item in this array is for 1, the second for 3,
   * the third for 5, and so on. Each item has nonzero value if the
   * corresponding number is nonprime.
   */
  mark_t* marks;
  /* The length of marks. */
  size_t n_marks;
  /* Copy configuration to static variables. */
  a = _a;
  b = _b;
  n_threads = _n_threads;
  verbose = _verbose;
  /* Allocate spaces for markings. */
  n_marks = alloc_marks(&marks);
  /* Simple, ancient algorithm comes here: Sieve of Eratosthenes. */
  sieve_mark_iter(n_marks, marks);
  /* Filter the result to count and print the prime numbers between a and b. */
  n_prime = sieve_filter(marks, n_marks);
  /* Set them free. */
  free(marks);
  return n_prime;
}

size_t alloc_marks(mark_t** marks) {
  size_t n_marks = b / 2;
  *marks = (mark_t*) calloc(n_marks, sizeof(mark_t));
  return n_marks;
}

void sieve_mark_iter(const size_t n_marks, mark_t* const marks) {
  /* The upper bound for the loop. */
  unsigned long sqrt_b = (unsigned long) sqrt((double) b);
  unsigned long idx_sqrt_b = sqrt_b / 2;
  /* Temporary loop variable which is used for array index. */
  size_t i;
  /* Task queue for marking multiples. */
  taskqueue_t queue;
  /* The array of IDs of threads generated in this function. */
  pthread_t* threads;
  /* Flag if multithreaded searching will be used. */
  int multithreaded = n_threads > 1;
  if (multithreaded) {
    /* Allocate spaces for thread IDs. Except for the main thread! */
    threads = (pthread_t*) calloc(n_threads, sizeof(pthread_t));
    /* Initialize the task queue. */
    if (taskqueue_init(&queue, 16 * n_threads, sieve_mark_routine) == NULL) {
      /* Uh oh, it failed. */
      return;
    }
    {
      /* Create threads to work on the task queue. */
      int i;
      for (i = 0; i < n_threads; ++i) {
        pthread_create(&threads[i], NULL, taskqueue_thread, &queue);
      }
    }
  }
  for (i = 1; i < idx_sqrt_b; ++i) {
    if (marks[i]) {
      /* Skip prime numbers. */
      continue;
    }
    /* Not marked as non-prime number yet. */
    /* Prepare argument for marking routine. */
    markarg_t* arg;
    arg = (markarg_t*) malloc(sizeof(markarg_t));
    arg->i = i;
    arg->marks = marks;
    arg->n_marks = n_marks;
    arg->b = b;
    if (multithreaded) {
      /* Push a new task to the task queue. */
      while (1) {
        /* Continue the loop only if the task queue is full so that the new
         * task can be pushed to the queue. */
        if (taskqueue_push(&queue, arg) != taskqueue_queue_full) {
          break;
        }
      }
    } else {
      /* Just call the marking routine. */
      sieve_mark_routine(arg);
    }
  }
  if (multithreaded) {
    /* No more task to push to task queue. Gracefully terminate the task queue.
     * Worker threads will keep working until the queue is exhausted. */
    taskqueue_terminate(&queue, threads, n_threads);
  }
}

void* sieve_mark_routine(void* _arg) {
  markarg_t* arg = _arg;
  /* The index given for this task. */
  const size_t i = arg->i;
  /* Steps for next multiple. */
  const size_t step = 2 * i + 1;
  /* The length of mark array. */
  const size_t n_marks = arg->n_marks;
  /* The first index to be marked. Since for number n = 2k + 1, the first
   * number to mark is n^2 = (2k + 1)^2 = 4k^2 + 4k + 1 = 2(2k^2 + 2k) + 1.
   * Here, k is i. */
  const size_t idx_first = 2 * i * i + 2 * i;
  /* Mark the multiples of the number which maps to arg->marks[arg->i]. */
  size_t idx;
  for (idx = idx_first; idx < n_marks; idx += step) {
    arg->marks[idx] = 1;
  }
  /* Arguments is not needed anymore. */
  free(arg);
  return NULL;
}

size_t sieve_filter(const mark_t* marks, const size_t n_marks) {
  /* The number of prime numbers between a and b. */
  size_t n_prime = 0;
  /* Array index of the number which is being tested. */
  unsigned long idx;
  /* The first number for the test. */
  const unsigned long first = (a % 2 == 0) ? a + 1 : a + 2;
  /* The first index for the test. */
  const size_t idx_first = first / 2;
  /* The only even prime number 2 was not 'searched'. Let's include this. */
  if (a == 1) {
    ++n_prime;
    if (verbose) {
      puts("2");
    }
  }
  for (idx = idx_first; idx < n_marks; ++idx) {
    if (!marks[idx]) {
      /* The number which maps to marks[idx] is prime number. */
      ++n_prime;
      if (verbose) {
        printf("%lu\n", 2 * idx + 1);
      }
    }
  }
  return n_prime;
}
