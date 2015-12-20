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
  return 0;
}

template <typename T> int list_append(T value, list_t<T>* list) {
  return 0;
}

template <typename T> int list_remove(listnode_t<T>* node, list_t<T>* list) {
  return 0;
}

template <typename T> void list_free(list_t<T>* list) {
}


} // namespace multicore

#endif  // MULTICORE_LIST_H_
