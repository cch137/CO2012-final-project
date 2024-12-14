#include "utils.h"
#include "doubly_linked_list.h"
#include <stdio.h>

static inline void join_list_node(DLNode *left_node, DLNode *right_node);
static inline void break_list_node(DLNode *left_node, DLNode *right_node);

DLNode *create_list_node(char *data)
{
  DLNode *node = malloc(sizeof(DLNode));
  if (!node)
    EXIT_ON_MEMORY_ERROR();
  node->data = dbutil_strdup(data);
  node->prev = NULL;
  node->next = NULL;
  return node;
}

DLList *create_list()
{
  DLList *list = malloc(sizeof(DLList));
  if (!list)
    EXIT_ON_MEMORY_ERROR();
  list->head = NULL;
  list->tail = NULL;
  list->length = 0;
  return list;
}

void free_list_node(DLNode *node)
{
  if (!node)
    return;
  break_list_node(node, node->next);
  break_list_node(node->prev, node);
  free(node->data);
  free(node);
}

void free_list(DLList *list)
{
  if (!list)
    return;

  DLNode *curr = list->head;

  while (curr)
  {
    list->head = curr->next;
    free_list_node(curr);
    curr = list->head;
  }

  free(list);
}

static inline void join_list_node(DLNode *left_node, DLNode *right_node)
{
  if (left_node)
    left_node->next = right_node;
  if (right_node)
    right_node->prev = left_node;
}

static inline void break_list_node(DLNode *left_node, DLNode *right_node)
{
  if (left_node)
    left_node->next = NULL;
  if (right_node)
    right_node->prev = NULL;
}

db_size_t lpush(DLList *list, DLNode *node)
{
  if (!list)
    return 0;

  while (node)
  {
    join_list_node(node, list->head);
    list->head = node;
    list->length++;
    node = node->prev;
  }

  if (list->head)
  {
    list->head->prev = NULL;
  }

  if (!list->tail)
  {
    list->tail = list->head;
  }

  return list->length;
}

DLNode *lpop(DLList *list)
{
  if (!list || !list->head)
    return NULL;

  DLNode *removed_node = list->head;

  list->head = removed_node->next;
  break_list_node(removed_node, list->head);
  --list->length;

  if (!list->head)
  {
    list->tail = NULL;
  }

  return removed_node;
}

DLList *lpop_n(DLList *list, db_size_t count)
{
  if (!list || !list->head || !count)
    return NULL;

  DLNode *curr = NULL;
  DLList *reply_list = create_list();

  while (count--)
  {
    curr = lpop(list);
    if (curr)
    {
      rpush(reply_list, curr);
    }
    else
    {
      break;
    }
  }

  return reply_list;
}

db_size_t rpush(DLList *list, DLNode *node)
{
  if (!list || !node)
    return 0;

  while (node)
  {
    join_list_node(list->tail, node);
    list->tail = node;
    list->length++;
    node = node->next;
  }

  if (!list->head)
  {
    list->head = list->tail;
  }

  return list->length;
}

DLNode *rpop(DLList *list)
{
  if (!list || !list->tail)
    return NULL;

  DLNode *removed_node = list->tail;

  list->tail = removed_node->prev;
  break_list_node(list->tail, removed_node);
  --list->length;

  if (!list->tail)
  {
    list->head = NULL;
  }

  return removed_node;
}

DLList *rpop_n(DLList *list, db_size_t count)
{
  if (!list || !list->tail || !count)
    return NULL;

  DLNode *curr = NULL;
  DLList *reply_list = create_list();

  while (count--)
  {
    curr = rpop(list);
    if (curr)
    {
      rpush(reply_list, curr);
    }
    else
    {
      break;
    }
  }

  return reply_list;
}

DLList *lrange(const DLList const *list, db_size_t start, db_size_t stop)
{
  DLList *reply_list = create_list();

  if (!list)
  {
    return reply_list;
  }

  if (stop == DB_SIZE_MAX || stop > list->length - 1)
  {
    stop = list->length - 1;
  }

  if (start > stop || start >= list->length)
  {
    return NULL;
  }

  // The new node must be initialized to NULL,
  // as it will be assigned to the reply list regardless of whether it has been created.
  DLNode *new_node = NULL;
  DLNode *last_new_node = NULL;
  DLNode *curr_node;
  db_size_t index;

  if (start > list->length - 1 - stop)
  {
    curr_node = list->tail;
    index = list->length - 1;
    while (index != stop && curr_node)
    {
      curr_node = curr_node->prev;
      --index;
    }
    while (index >= start && curr_node)
    {
      new_node = create_list_node(curr_node->data);
      join_list_node(new_node, last_new_node);
      last_new_node = new_node;
      if (!reply_list->tail)
        reply_list->tail = new_node;
      curr_node = curr_node->prev;
      --index;
    }
    reply_list->head = new_node;
  }
  else
  {
    curr_node = list->head;
    index = 0;
    while (index != start && curr_node)
    {
      curr_node = curr_node->next;
      ++index;
    }
    while (index <= stop && curr_node)
    {
      new_node = create_list_node(curr_node->data);
      join_list_node(last_new_node, new_node);
      last_new_node = new_node;
      if (!reply_list->head)
        reply_list->head = new_node;
      curr_node = curr_node->next;
      ++index;
    }
    reply_list->tail = new_node;
  }

  reply_list->length = stop - start + 1;

  return reply_list;
}
