#include <pthread.h>

/* Type definition for (circular) task queue. */
typedef struct {
  /* Collection of pointers to task arguments to be passed to threads. */
  void** task_args;
  /* Function pointer to task routine. */
  void* (*routine)(void*);
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
 * Initializes a task queue with length of len and task routine. Returns the
 * pointer to initialized task queue if succeded, otherwise NULL.
 */
taskqueue_t* taskqueue_init(taskqueue_t* taskqueue, int len,
                            void* (*routine)(void*));
/*
 * Pushes a new task with argument arg to a task queue. Returns 0 if everything
 * went well, otherwise nonzero value.
 */
int taskqueue_push(taskqueue_t* taskqueue, void* arg);
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