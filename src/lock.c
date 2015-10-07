#include "lock.h"

#include <stdlib.h>

#define true 1
#define false 0

/* Returns the maximum of labels in a lock. Atomicity is not guaranteed.
 * Pointer to memory space for lock should be given as input. */
int max_label(lock_t* ptr_lock);

int lock_init(lock_t* ptr_lock, int n_threads) {
  ptr_lock->n_threads = n_threads;
  ptr_lock->choosing =
      (lock_choosing_t*) calloc(n_threads, sizeof(lock_choosing_t));
  ptr_lock->label = (lock_label_t*) calloc(n_threads, sizeof(lock_label_t));
  return (NULL == ptr_lock->choosing || NULL == ptr_lock->label);
}

void lock_on(lock_t* ptr_lock, int tid_i) {
  /* Pointer to array of choosing flags. */
  lock_choosing_t* ptr_choosings = ptr_lock->choosing;
  /* Pointer to array of labels. */
  lock_label_t* ptr_labels = ptr_lock->label;
  /* Pointer to my choosing flag. */
  lock_choosing_t* ptr_choosing_i = &ptr_choosings[tid_i];
  /* Pointer to my label. */
  lock_label_t* ptr_label_i = &ptr_labels[tid_i];
  /* My label. */
  lock_label_t label_i;
  /* Number of threads being used. */
  int n_threads = ptr_lock->n_threads;
  /* Another thread ID. */
  int tid_j;

  *ptr_choosing_i = true;
  *ptr_label_i = label_i = 1 + max_label(ptr_lock);
  *ptr_choosing_i = false;

  for (tid_j = 0; tid_j < n_threads; ++tid_j) {
    /* Pointer to other's choosing flag. */
    lock_choosing_t* ptr_choosing_j = &ptr_choosings[tid_j];
    /* Pointer to other's label. */
    lock_label_t* ptr_label_j = &ptr_labels[tid_j];
    while (*ptr_choosing_j) {
      /* no-op */
    }
    while (1) {
      lock_label_t label_j = *ptr_label_j;
      if (label_j == 0 ||
          (label_j > label_i || (label_j == label_i && tid_j >= tid_i))) {
        break;
      }
    }
  }
}

int max_label(lock_t* ptr_lock) {
  /* Number of threads being used. */
  int n_threads = ptr_lock->n_threads;
  /* Another thread ID. */
  int tid_j;
  /* Maximum of labels. */
  int max = 0;
  for (tid_j = 0; tid_j < n_threads; ++tid_j) {
    if (max < ptr_lock->label[tid_j]) {
      max = ptr_lock->label[tid_j];
    }
  }
  return max;
}

void lock_off(lock_t* ptr_lock, int thread_id) {
  ptr_lock->label[thread_id] = 0;
}

void lock_destroy(lock_t* ptr_lock) {
  free(ptr_lock->choosing);
  free(ptr_lock->label);
}
