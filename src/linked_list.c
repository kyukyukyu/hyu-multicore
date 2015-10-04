#include "linked_list.h"

#include <stdlib.h>

void list_init(list_t* ptr_list) {
  ptr_list->length = 0;
  ptr_list->head = NULL;
}

int list_insert(void* elem, list_t* ptr_list, unsigned long idx) {
  /* Pointer to new node to be inserted into the list. */
  list_node_t* new_node;
  /* Pointer to node whose next node will be updated. */
  list_node_t* node;
  if (!ptr_list) {
    /* Invalid list pointer. */
    return 1;
  }
  new_node = (list_node_t*) malloc(sizeof(list_node_t));
  new_node->elem = elem;
  if (idx == 0) {
    /* Pointer to head node should be updated. */
    new_node->next = ptr_list->head;
    ptr_list->head = new_node;
    goto increase_len;
  }
  /* Find the node at position (idx - 1). */
  node = ptr_list->head;
  while (--idx && node) {
    node = node->next;
  }
  if (!node) {
    /* Invalid index. */
    return 1;
  }
  /* Insert new node at position idx. */
  new_node->next = node->next;
  node->next = new_node;
increase_len:
  ++ptr_list->length;
  return 0;
}

void* list_delete_first(list_t* ptr_list, int(*criteria)(void*)) {
  /* Pointer to node being worked on. */
  list_node_t* node;
  /* Pointer to node that will be deleted. */
  list_node_t* node_del;
  /* Pointer to element that will be returned. */
  void* ret;
  /* Examine the head node. */
  node = ptr_list->head;
  if (criteria(node->elem)) {
    /* Pointer to head node should be updated. */
    node_del = node;
    ptr_list->head = node_del->next;
    goto node_popped;
  }
  /* Find the node whose next node satisfies the criteria. */
  while (node->next && !criteria(node->next->elem)) {
    node = node->next;
  }
  node_del = node->next;
  if (NULL == node_del) {
    /* Node that satisfies the criteria does not exist. */
    return NULL;
  }
  node->next = node_del->next;
node_popped:
  ret = node_del->elem;
  free(node_del);
  --ptr_list->length;
  return ret;
}

void list_delete_multiple(list_t* ptr_list, int(*criteria)(void*),
                          void(*routine)(void*)) {
  /* Pointer to node being worked on. */
  list_node_t* node;
  /* Examine the head node. */
  node = ptr_list->head;
  if (criteria(node->elem)) {
    /* Pointer to head node should be updated. */
    ptr_list->head = NULL;
    goto list_cut;
  }
  /* Find the node whose next node satisfies the criteria. */
  while (node->next && !criteria(node->next->elem)) {
    node = node->next;
  }
  if (NULL == node->next) {
    /* Node that satisfies the criteria does not exist. */
    return;
  }
  node->next = NULL;
list_cut:
  while (node) {
    /* Node to be deleted at this time. */
    list_node_t* node_del = node;
    routine(node_del->elem);
    node = node->next;
    free(node_del);
    --ptr_list->length;
  }
}
