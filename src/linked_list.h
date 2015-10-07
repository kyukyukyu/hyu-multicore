/*
 * Implementation of singly linked list of any type of data referenced by void*
 * type pointer. These operations are defined:
 *
 * - Reading element at arbitrary position. Takes O(i).
 * - Insertion of new element at arbitrary position i. Takes O(i).
 * - Deletion of the first element in the list that satisfies arbitrary
 *   criteria. Takes O(i), where i is the index of element.
 * - Deletion of multiple elements from the first element that satisfies
 *   arbitrary criteria to the last element from the list, with running
 *   arbitrary routine at each deletion of element. Takes O(n - i), where n is
 *   the length of list, and i is the index of element that satisfies the
 *   criteria.
 *
 * Author: Sanggyu Nam <pokeplus@gmail.com>
 */
#ifndef MULTICORE_LINKEDLIST_H_
#define MULTICORE_LINKEDLIST_H_

/* Typedef for nodes in linked lists. */
typedef struct node_t {
  /* Pointer to its next node. NULL if this is the last node. */
  struct node_t* next;
  /* Pointer to its element. */
  void* elem;
} list_node_t;
/* Typedef for linked lists. */
typedef struct {
  /* Pointer to its head node. NULL if this list is empty. */
  list_node_t* head;
  /* The length of this list. */
  unsigned long length;
} list_t;
/* Typedef for criteria closures. */
typedef struct {
  /* Function pointer. The first argument is for element, and the second
   * argument is for environment. */
  int(*func)(void* elem, void* env);
  /* Pointer to memory space for bound environment. */
  void* env;
} crit_closure_t;

/* Initializer function for a linked list. Pointer to list should be given as
 * input. */
void list_init(list_t* ptr_list);
/* Returns the element at arbitrary position. Pointer to list and index of the
 * position should be given as input. If invalid index is given, NULL pointer
 * will be returned. */
void* list_at(const list_t* ptr_list, unsigned long idx);
/* Inserts a new element at arbitrary position into a linked list. Pointer to
 * new element, pointer to list, and index of position should be given as
 * input. Returns nonzero value if inserting was not successful. */
int list_insert(void* elem, list_t* ptr_list, unsigned long idx);
/* Deletes the first element that satisfies arbitrary criteria from a linked
 * list. Pointer to list, and pointer to criteria closure should be given as
 * input. The function in criteria closure should return nonzero value if an
 * element satisfies the criteria. Returns the pointer to deleted element if
 * any, otherwise null pointer. */
void* list_delete_first(list_t* ptr_list, crit_closure_t* criteria);
/* Deletes multiple elements from the element that satisfies arbitrary criteria
 * to the last element from a linked list, with running arbitrary routine at
 * each deletion of element. Pointer to list, pointer to criteria closure, and
 * pointer to routine function should be given as input. In contrast to
 * list_delete_first(), this does not return pointers to deleted elements.
 * Instead, caller can do something with these pointers with routine
 * function. */
void list_delete_multiple(list_t* ptr_list, crit_closure_t* criteria,
                          void(*routine)(void*));

#endif  /* MULTICORE_LINKEDLIST_H_ */
