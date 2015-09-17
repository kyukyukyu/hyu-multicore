#include "taskqueue.h"
#include <stdlib.h>

taskqueue_t* taskqueue_init(taskqueue_t* taskqueue, int len,
                            void* (*routine)(void*)) {
  /* Initialize. */
  taskqueue->n_items = 0;
  taskqueue->len = len;
  taskqueue->head = 0;
  taskqueue->tail = 0;
  taskqueue->term_flag = 0;
  taskqueue->routine = routine;
  /* Allocate space for task arguments. */
  taskqueue->task_args = (void**) malloc(len * sizeof(void*));
  if (pthread_mutex_init(&taskqueue->lock, NULL) != 0 ||
      pthread_cond_init(&taskqueue->cond, NULL) != 0 ||
      taskqueue->task_args == NULL) {
    taskqueue_free(taskqueue);
    return NULL;
  }
  return taskqueue;
}

int taskqueue_push(taskqueue_t* taskqueue, void* arg) {
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
      void* arg = queue->task_args[queue->head];
      int next_head;
      next_head  = queue->head + 1;
      next_head = (next_head == queue->len) ? 0 : next_head;
      queue->head = next_head;
      --queue->n_items;
      /* Second, release the mutex. Nothing left to do with the queue. */
      pthread_mutex_unlock(&queue->lock);
      /* Third, call the task routine. */
      (queue->routine)(arg);
    } else {
      /* Empty queue and termination flag is on? Okay, let's call it a day! */
      break;
    }
  }
  pthread_mutex_unlock(&queue->lock);
  pthread_exit(NULL);
  return NULL;
}
