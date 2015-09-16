#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

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
 * Frees resources used by task queue. If taskqueue is dynamically allocated,
 * it is caller's duty to free it. Returns 0 if everything went well, otherwise
 * nonzero value.
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
 * Finds prime numbers between a and b with a number of threads (including the
 * main one), and returns the number of found prime numbers. If verbose is
 * nonzero value, found prime numbers will be printed to stdout, one number in
 * one line.
 */
size_t find_prime_numbers(const unsigned long a,
                          const unsigned long b,
                          const int n_threads,
                          const int verbose);
/*
 * Allocates space for the array of marks which is used for seqs in
 * find_prime_numbers(), and returns the length of array. The upper bound of
 * search range and pointer to seqs should be provided.
 */
size_t alloc_marks(const unsigned long b, mark_t** marks);
/*
 * For every odd number smaller than sqrt(b), marks the multiples of the number
 * non-prime. In multi-threaded fashion. n_threads includes the main thread.
 */
void sieve_mark_iter(const size_t n_marks, const unsigned long b,
                     const int n_threads, mark_t* const marks);
/*
 * Thread routine which marks multiples of an odd-number which is not marked
 * as non-prime yet.
 */
void* sieve_mark_routine(markarg_t* arg);
/*
 * Filter the result of sieve_mark_iter() to count the number of prime numbers
 * between a and b, and returns the number of counted prime numbers. If verbose
 * is nonzero value, counted prime numbers are printed to stdout, one number in
 * one line.
 */
size_t sieve_filter(const mark_t* marks, const size_t n_marks,
                    const unsigned long a, const unsigned long b,
                    const int verbose);

taskqueue_t* taskqueue_init(taskqueue_t* taskqueue, int len) {
  /* Initialize. */
  taskqueue->n_items = 0;
  taskqueue->len = len;
  taskqueue->head = 0;
  taskqueue->tail = 0;
  taskqueue->term_flag = 0;
  /* Allocate space for task arguments. */
  taskqueue->task_args = (markarg_t**) malloc(len * sizeof(markarg_t*));
  if (pthread_mutex_init(&taskqueue->lock, NULL) != 0 ||
      pthread_cond_init(&taskqueue->cond, NULL) != 0 ||
      taskqueue->task_args == NULL) {
    taskqueue_free(taskqueue);
    return NULL;
  }
  return taskqueue;
}

int taskqueue_push(taskqueue_t* taskqueue, markarg_t* arg) {
  /* Error occured in this function. If no errors, it has 0. */
  int err = 0;

  if (!taskqueue || !arg) {
    /* Invalid arguments. */
    return taskqueue_invalid;
  }
  if (pthread_mutex_lock(&taskqueue->lock) != 0) {
    /* Locking was not successful. */
    return taskqueue_lock_failure;
  }

  /* Now we acquired the lock. Time to do something real. */
  {
    /* Next position for tail in the queue. */
    int next_tail;
    next_tail = taskqueue->tail + 1;
    next_tail = (next_tail == taskqueue->len) ? 0 : next_tail;
    if (taskqueue->n_items == taskqueue->len) {
      /* Queue is full. */
      err = taskqueue_queue_full;
      goto finally_push;
    }
    /* Push new task to the queue. */
    taskqueue->task_args[taskqueue->tail] = arg;
    taskqueue->tail = next_tail;
    ++taskqueue->n_items;
    /* Wake up one thread and let them grab a task from queue. */
    if (pthread_cond_signal(&taskqueue->cond) != 0) {
      err = taskqueue_lock_failure;
      goto finally_push;
    }
  }

finally_push:
  /* Just like finally block in many other languages w/exception handling. */
  if (pthread_mutex_unlock(&taskqueue->lock) != 0) {
    /* Unlocking was not successful. */
    return taskqueue_lock_failure;
  }

  return err;
}

int taskqueue_terminate(taskqueue_t* taskqueue, pthread_t* threads,
                        int n_threads) {
  int err = 0;
  if (!taskqueue) {
    return taskqueue_invalid;
  }
  if (pthread_mutex_lock(&taskqueue->lock) != 0) {
    /* Locking was not successful. */
    return taskqueue_lock_failure;
  }
  /* Set termination flag for the queue. */
  taskqueue->term_flag = 1;
  /* Wake all the threads. Since the threads lock the mutex and wait for signal
   * only if the queue is empty and termination flag is not set, this does not
   * cause race on the queue.
   * Also, the mutex should be released so that threads can work on the queue.
   */
  if (pthread_cond_broadcast(&taskqueue->cond) != 0 ||
      pthread_mutex_unlock(&taskqueue->lock) != 0) {
    err = taskqueue_lock_failure;
    goto finally_terminate;
  }
  /* Join all the threads so that this function returns after they are
   * terminated. */
  {
    int i;
    for (i = 0; i < n_threads; ++i) {
      if (pthread_join(threads[i], NULL) != 0) {
        err = taskqueue_thread_failure;
      }
    }
  }
finally_terminate:
  if (!err) {
    /* Only when everything went well. */
    taskqueue_free(taskqueue);
  }
  return err;
}

int taskqueue_free(taskqueue_t* taskqueue) {
  if (!taskqueue) {
    return -1;
  }
  if (taskqueue->task_args) {
    /* Free resources only if taskqueue is initialized correctly. */
    free(taskqueue->task_args);
    pthread_cond_destroy(&taskqueue->cond);
    pthread_mutex_destroy(&taskqueue->lock);
  }
  return 0;
}

void* taskqueue_thread(void* taskqueue) {
  taskqueue_t* queue = taskqueue;
  /* Loop forever until termination. */
  while (1) {
    /* Lock the mutex on the queue to wait for the condition variable. */
    pthread_mutex_lock(&queue->lock);
    /* Wait on condition variable. This will wake up when signal was sent from
     * main thread, which is for new task or termination.
     * When waiting is over, the mutex is owned by this thread.
     * If the queue is empty and termination flag is off, keep waiting. */
    while (queue->n_items == 0 && !queue->term_flag) {
      pthread_cond_wait(&queue->cond, &queue->lock);
    }
    /* Now the queue is not empty or termination flag is on.
     * Also, it is safe to work on the queue since mutex is owned by this
     * thread. */
    if (queue->n_items > 0) {
      /* Queue is not empty? Work! */
      /* First, pop a task from the queue. */
      markarg_t* arg = queue->task_args[queue->head];
      int next_head;
      next_head  = queue->head + 1;
      next_head = (next_head == queue->len) ? 0 : next_head;
      queue->head = next_head;
      --queue->n_items;
      /* Second, release the mutex. Nothing left to do with the queue. */
      pthread_mutex_unlock(&queue->lock);
      /* Third, call the marking routine. */
      sieve_mark_routine(arg);
    } else {
      /* Empty queue and termination flag is on? Okay, let's call it a day! */
      break;
    }
  }
  pthread_mutex_unlock(&queue->lock);
  pthread_exit(NULL);
  return NULL;
}

int main(void) {
  /* Lower bound for finding prime numbers. */
  const unsigned long a = 1;
  /* Upper bound for finding prime numbers. */
  const unsigned long b = 1000000000;  /* 10^9 */
  /* The number of threads to be used. */
  const int n_threads = 4;
  /* Flag for verbose finding. If nonzero, found prime numbers will be printed
   * to stdout, one number in one line. */
  const int verbose = 0;
  /* The number of prime numbers in the range. */
  size_t n_prime;
  n_prime = find_prime_numbers(a, b, n_threads, verbose);
  printf("Total number of prime numbers between %lu and %lu is %lu.\n",
         a, b, n_prime);
  return 0;
}

size_t find_prime_numbers(const unsigned long a,
                          const unsigned long b,
                          const int n_threads,
                          const int verbose) {
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
  n_marks = alloc_marks(b, &marks);
  /* Simple, ancient algorithm comes here: Sieve of Eratosthenes. */
  sieve_mark_iter(n_marks, b, n_threads, marks);
  n_prime = sieve_filter(marks, n_marks, a, b, verbose);
  /* Set them free. */
  free(marks);
  return n_prime;
}

size_t alloc_marks(const unsigned long b, mark_t** marks) {
  size_t n_marks = b / 2;
  *marks = (mark_t*) calloc(n_marks, sizeof(mark_t));
  return n_marks;
}

void sieve_mark_iter(const size_t n_marks, const unsigned long b,
                     const int n_threads, mark_t* const marks) {
  /* The upper bound for the loop. */
  unsigned long sqrt_b = (unsigned long) sqrt((double) b);
  unsigned long idx_sqrt_b = sqrt_b / 2;
  /* Temporary loop variable which is used for array index. */
  size_t i;
  /* Task queue for marking multiples. */
  taskqueue_t queue;
  /* The array of IDs of threads generated in this function. */
  pthread_t* threads;
  /* The number of threads except main one. */
  const int n_worker_threads = n_threads - 1;
  /* Allocate spaces for thread IDs. Except for the main thread! */
  threads = (pthread_t*) calloc(n_worker_threads, sizeof(pthread_t));
  /* Initialize the task queue. */
  if (taskqueue_init(&queue, 64 * n_worker_threads) == NULL) {
    /* Uh oh, it failed. */
    return;
  }
  {
    /* Create threads to work on the task queue. */
    int i;
    for (i = 0; i < n_worker_threads; ++i) {
      pthread_create(&threads[i], NULL, taskqueue_thread, &queue);
    }
  }
  for (i = 1; i < idx_sqrt_b; ++i) {
    if (!(marks[i])) {
      /* Not marked as non-prime number yet. */
      /* Prepare argument for marking routine. */
      markarg_t* arg;
      arg = (markarg_t*) malloc(sizeof(markarg_t));
      arg->i = i;
      arg->marks = marks;
      arg->n_marks = n_marks;
      arg->b = b;
      /* Push a new task to the task queue. */
      while (1) {
        /* Continue the loop only if the task queue is full so that the new
         * task can be pushed to the queue. */
        if (taskqueue_push(&queue, arg) != taskqueue_queue_full) {
          break;
        }
      }
    }
  }
  /* No more task to push to task queue. Gracefully terminate the task queue.
   * Worker threads will keep working until the queue is exhausted. */
  taskqueue_terminate(&queue, threads, n_worker_threads);
}

void* sieve_mark_routine(markarg_t* arg) {
  /* Steps for next multiple. */
  const size_t step = 2 * arg->i + 1;
  /* The length of mark array. */
  const size_t n_marks = arg->n_marks;
  /* Mark the multiples of the number which maps to arg->marks[arg->i]. */
  size_t i = arg->i;
  for (i = arg->i; i < n_marks; i += step) {
    arg->marks[i] = 1;
    i += step;
  }
  /* Arguments is not needed anymore. */
  free(arg);
  return NULL;
}

size_t sieve_filter(const mark_t* marks, const size_t n_marks,
                    const unsigned long a, const unsigned long b,
                    const int verbose) {
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
