#ifndef MULTICORE_LIST_H_
#define MULTICORE_LIST_H_

namespace multicore {

// Type for nodes in doubly linked lists. Insertion is only allowed at the tail
// of list. Deletion is allowed at anywhere.
template <typename T> struct listnode_t {
  // Pointer to previous node.
  struct listnode_t<T>* prev;
  // Pointer to next node.
  struct listnode_t<T>* next;
  // Value of this node.
  T value;
};
// Type for doubly linked lists.
template <typename T> struct list_t {
  // Pointer to head node.
  struct listnode_t<T>* head;
  // Pointer to tail node.
  struct listnode_t<T>* tail;
  // Number of nodes in this list.
  unsigned long n_nodes;
};

// Initializes an empty list.
template <typename T> int list_init(list_t<T>* list);
// Appends a new node with given value to a list.
template <typename T> int list_append(T value, list_t<T>* list);
// Removes a node from a list. Note that pointer to node should be given.
template <typename T> int list_remove(listnode_t<T>* node, list_t<T>* list);
// Frees a list.
template <typename T> void list_free(list_t<T>* list);

template <typename T> int list_init(list_t<T>* list) {
  list->head = nullptr;
  list->tail = nullptr;
  list->n_nodes = 0;
  return 0;
}

template <typename T> int list_append(T value, list_t<T>* list) {
  // Create a new node.
  listnode_t<T>* node = new listnode_t<T>;
  node->value = value;
  node->next = nullptr;
  // Run test-and-set for tail of list with new node.
  node->prev = __sync_lock_test_and_set(&list->tail, node);
  if (nullptr == node->prev) {
    list->head = node;
  } else {
    // Update next pointer of previous node.
    node->prev->next = node;
  }
  __sync_add_and_fetch(&list->n_nodes, 1);
  return 0;
}

template <typename T> int list_remove(listnode_t<T>* node, list_t<T>* list) {
  if (0 == list->n_nodes) {
    // List is empty.
    return 1;
  }
  if (nullptr != node->prev) {
    node->prev->next = node->next;
  } else {
    // node is head of list.
    list->head = node->next;
  }
  if (nullptr != node->next) {
    node->next->prev = node->prev;
  } else {
    // node is tail of list.
    list->tail = node->prev;
  }
  delete node;
  __sync_add_and_fetch(&list->n_nodes, -1);
  return 0;
}

template <typename T> void list_free(list_t<T>* list) {
  listnode_t<T>* curr = list->head;
  while (curr) {
    listnode_t<T>* next = curr->next;
    delete curr;
    curr = next;
  }
}


} // namespace multicore

#endif  // MULTICORE_LIST_H_
