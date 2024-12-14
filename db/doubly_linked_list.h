#ifndef DB_DOUBLY_LINKED_LIST
#define DB_DOUBLY_LINKED_LIST

#include "types.h"

// Creates a new node for a doubly linked list with specified data
DLNode *create_list_node(char *data);

// Initializes a new, empty doubly linked list
DLList *create_list();

// Frees a list node
void free_list_node(DLNode *node);

// Free node and return node value
static inline char *extract_list_node(DLNode *node)
{
  if (!node)
    return NULL;

  char *data = node->data;
  node->data = NULL;
  free_list_node(node);

  return data;
}

// Frees an entire list and all of its nodes
void free_list(DLList *list);

// Pushes elements to the front of a list; last parameter must be NULL
db_size_t lpush(DLList *list, DLNode *node);

// Pops a elements from the front of a list
DLNode *lpop(DLList *list);

// Pops elements from the front of a list
DLList *lpop_n(DLList *list, db_size_t count);

// Pushes elements to the end of a list; last parameter must be NULL
db_size_t rpush(DLList *list, DLNode *node);

// Pops a element from the end of a list
DLNode *rpop(DLList *list);

// Pops elements from the end of a list
DLList *rpop_n(DLList *list, db_size_t count);

// Returns a sublist from the specified range of indices
// The `stop` index is inclusive, and if `stop` is -1, the entire list is returned
DLList *lrange(const DLList const *list, db_size_t start, db_size_t stop);

#endif
