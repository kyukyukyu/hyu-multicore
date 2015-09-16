#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define SEQ_SIZE (8 * sizeof(seq_t))

/* Type alias for bit sequences. */
typedef unsigned long seq_t;
/* Type definition for arguments to be passed to thread function for
 * sieve_map(). */
typedef struct {
  /* The number whose multiples will be marked as non-prime. */
  unsigned long i;
  /* Pointer to the head of bit sequence array. */
  seq_t* seqs;
  /* Length of array which seqs points to. */
  size_t n_seqs;
  /* The exclusive upper bound of searching prime numbers. */
  unsigned long b;
} markarg_t;
/* Type definition for (circular) task queue. */
typedef struct {
  /* Collection of pointers to task arguments to be passed to threads. */
  markarg_t** task_args;
  /* The length of this queue. */
  int len;
  /* The number of items in this queue. */
  int n_items;
  /* Position of head item in this queue. */
  int head;
  /* Position of tail item in this queue. */
  int tail;
  /* Mutex lock for this queue. */
  pthread_mutex_t lock;
  /* Condition variable for passing tasks to threads. */
  pthread_cond_t cond;
  /* Termination flag for this queue. Nonzero value means that this flag is
   * set. */
  int term_flag;
} taskqueue_t;
/* Type definition for errors returned by taskqueue functions. */
typedef enum {
  taskqueue_invalid = -1,
  taskqueue_lock_failure = -2,
  taskqueue_queue_full = -3,
  taskqueue_thread_failure = -4
} taskqueue_error_t;

/*
 * Initializes a task queue with length of len. Returns the pointer to
 * initialized task queue if succeded, otherwise NULL.
 */
taskqueue_t* taskqueue_init(taskqueue_t* taskqueue, int len);
/*
 * Pushes a new task with argument arg to a task queue. Returns 0 if everything
 * went well, otherwise nonzero value.
 */
int taskqueue_push(taskqueue_t* taskqueue, markarg_t* arg);
/*
 * Gracefully terminates the threads who are working on a task queue, and
 * releases the resources used by the queue. This means that threads will be
 * terminated after exhausting tasks in the task queue. Returns 0 if everything
 * went well and all the threads are terminated, otherwise nonzero value.
 */
int taskqueue_terminate(taskqueue_t* taskqueue, pthread_t* threads,
                        int n_threads);
/*
 * Frees resources used by task queue. Returns 0 if everything went well,
 * otherwise nonzero value.
 */
int taskqueue_free(taskqueue_t* taskqueue);
/*
 * Thread routine for running tasks. At first, this waits for any signal on a
 * taskqueue. When signal comes, this checks if task queue is empty. If the
 * queue is not empty, this grabs tasks and runs them one by one. Otherwise,
 * if termination flag is on for the queue, this terminates. If not, this waits
 * for signal again.
 */
void* taskqueue_thread(void* taskqueue);
/*
 * Finds prime numbers between a and b, stores them in prime_numbers (only if
 * prime_numbers is not NULL), and returns the number of found prime numbers.
 */
size_t find_prime_numbers(const unsigned long a,
                          const unsigned long b,
                          unsigned long** prime_numbers);
/*
 * Allocates space for the array of bit sequences which is used for seqs in
 * find_prime_numbers(), and returns the length of array. The upper bound of
 * search range and pointer to seqs should be provided.
 */
size_t alloc_seqs(const unsigned long b, seq_t** seqs);
/*
 * For every odd number smaller than sqrt(b), marks the multiples of the number
 * non-prime. In multi-threaded fashion.
 */
void sieve_map(const size_t n_seqs, const unsigned long b, seq_t* const seqs);
/*
 * Thread routine which marks multiples of an odd-number which is not marked
 * as non-prime yet.
 */
void* sieve_mark_routine(markarg_t* arg);
/*
 * Filter the result of sieve_map() to count the number of prime numbers
 * between a and b, and if required, fill the array of them. prime_numbers
 * should be NULL if filling the array is not required. Returns the number of
 * counted prime numbers.
 */
size_t sieve_filter(const seq_t* seqs, const size_t n_seqs,
                    const unsigned long a, const unsigned long b,
                    unsigned long* const prime_numbers);
/*
 * Prints the list of prime numbers. The array of prime numbers and the number
 * of them should be provided.
 */
void print_prime_numbers(const unsigned long* prime_numbers,
                         const size_t n_prime);

int main(void) {
  /* Lower bound for finding prime numbers. */
  const unsigned long a = 1;
  /* Upper bound for finding prime numbers. */
  const unsigned long b = 1000000000;  /* 10^9 */
  /* The number of prime numbers in the range. */
  size_t n_prime;
  /* The list of found prime numbers. */
  unsigned long* prime_numbers;
  n_prime = find_prime_numbers(a, b, &prime_numbers);
  print_prime_numbers(prime_numbers, n_prime);
  printf("Total number of prime numbers between %lu and %lu is %lu.\n",
         a, b, n_prime);
  /* Set them free. */
  free(prime_numbers);
  return 0;
}

size_t find_prime_numbers(const unsigned long a,
                          const unsigned long b,
                          unsigned long** prime_numbers) {
  /* The number of prime numbers between a and b. */
  size_t n_prime = 0;
  /*
   * Flag that says if found prime numbers should be stored in prime_numbers.
   */
  int store_numbers = !(prime_numbers == NULL);
  /*
   * Pointer to the array of bit sequences. Each bit represents if an odd
   * number is prime or not. A bit with bigger significance represents bigger
   * number, and the sequence of bigger numbers is stored at bigger index. For
   * example, if sizeof(seq_t) is 32, seqs[0] is a bit sequence whose LSB
   * represents whether 1 is prime, and MSB represents whether 63 is prime.
   * Similarly, LSB of seqs[1] for 65, and MSB of seqs[1] for 127. Each bit is
   * set 1 if corresponding number is not prime, or 0 otherwise.
   */
  seq_t* seqs;
  /* The length of seqs. */
  size_t n_seqs;
  if (store_numbers) {
    /*
     * Allocate space for the list of prime numbers. The number of prime
     * numbers is unknown here. Therefore, allocate space for the half of
     * numbers between a and b.
     */
    *prime_numbers =
        (unsigned long*) malloc((b - a) / 2 * sizeof(unsigned long));
  }
  n_seqs = alloc_seqs(b, &seqs);
  /* Simple, ancient algorithm comes here: Sieve of Eratosthenes. */
  sieve_map(n_seqs, b, seqs);
  n_prime = sieve_filter(seqs, n_seqs, a, b,
                         store_numbers ? *prime_numbers : NULL);
  /* Set them free. */
  free(seqs);
  return n_prime;
}

size_t alloc_seqs(const unsigned long b, seq_t** seqs) {
  /* Rounding up b / SEQ_SIZE. */
  size_t n_seqs = (b + SEQ_SIZE / 2) / SEQ_SIZE;
  *seqs = (seq_t*) calloc(n_seqs, sizeof(seq_t));
  return n_seqs;
}

void sieve_map(const size_t n_seqs, const unsigned long b, seq_t* const seqs) {
  /* The upper bound for the loop. */
  unsigned long sqrt_b = (unsigned long) sqrt((double) b);
  /*
   * The number to be checked if prime within the first loop. For the second
   * loop, this is the index of thread to be joined.
   */
  unsigned long i;
  /* Bit mask to use within the loop. Initial value is for number 3. */
  seq_t mask = 0x1 << 1;
  /* The index of bit sequence to look at within the loop. */
  size_t seq_idx = 0;
  /* The array of IDs of threads generated in this function. */
  pthread_t* threads;
  /* The number of threads to be generated. */
  const size_t n_threads = (sqrt_b - 3 + 1) / 2;
  /* Allocate spaces for thread IDs. +1 in () is for rounding up. */
  threads = (pthread_t*) calloc(n_threads, sizeof(pthread_t));
  for (i = 3; i < sqrt_b; i += 2) {
    if (!(seqs[seq_idx] & mask)) {
      /* Not marked as non-prime number yet. */
      /* Prepare argument for marking routine. */
      markarg_t* arg;
      arg = (markarg_t*) malloc(sizeof(markarg_t));
      arg->i = i;
      arg->seqs = seqs;
      arg->n_seqs = n_seqs;
      arg->b = b;
    }
  }
  /* Send a signal that iterating over numbers is over. */
}

void* sieve_mark_routine(markarg_t* arg) {
  /* Number whose multiples will be marked. */
  const unsigned long i = arg->i;
  /* The number of steps in bit sequences when moving to next multiple. */
  const int step = (i % (SEQ_SIZE * 2)) / 2;
  /* The number of jumps in the array of bit sequences when moving to next
   * multiple. */
  const int jump = i / (SEQ_SIZE * 2);
  /* Bit mask to be used for marking. */
  seq_t mask = 0x1 << step;
  /* Index of bit sequence to be accessed and used for marking. */
  size_t seq_idx = jump;
  /* The length of bit sequence array. */
  const size_t n_seqs = arg->n_seqs;
  /* The array of bit sequences to be updated. */
  seq_t* seqs = arg->seqs;
  /*
   * Assume that an integer variable named j is declared. j has the number
   * which is a multiple of i and j will be marked as non-prime number in this
   * iteration. The value of j can be computed with mask and seq_idx.
   * For readability, declaring j is the better choice. But this time,
   * performance matters...
   */
  while (1) {
    /* Let j be the next multiple of i. i.e. We are doing j += i */
    seq_idx += jump;
    if (!(mask << step)) {
      ++seq_idx;
    }
    if (seq_idx >= n_seqs) {
      /* j got bigger than the biggest number in the array of bit sequences. */
      break;
    }
    /* Circular shift-left */
    mask = (mask << step) | (mask >> (SEQ_SIZE - step));
    /* Now j is the next multiple. Mark it non-prime. */
    seqs[seq_idx] |= mask;
  }
  /* Arguments is not needed anymore. */
  free(arg);
  return NULL;
}

size_t sieve_filter(const seq_t* seqs, const size_t n_seqs,
                    const unsigned long a, const unsigned long b,
                    unsigned long* const prime_numbers) {
  size_t n_prime = 0;
  return n_prime;
}

void print_prime_numbers(const unsigned long* prime_numbers,
                         const size_t n_prime) {
  size_t i;
  for (i = 0; i < n_prime; ++i) {
    printf("%lu\n", prime_numbers[i]);
  }
}
