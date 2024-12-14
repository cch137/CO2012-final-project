#ifndef DB_TYPES_H
#define DB_TYPES_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define DB_ERR_DB_IS_CLOSED "ERR database is closed"
#define DB_ERR_ARG_ERROR "ERR wrong arguments "
#define DB_ERR_WRONGTYPE "WRONGTYPE Operation against a key holding the wrong kind of value"
#define DB_ERR_NONEXISTENT_KEY "ERR no such key"
#define DB_ERR_UNKNOWN_COMMAND "ERR unknown command"

typedef enum db_type_t
{
  DB_TYPE_NULL,
  DB_TYPE_ERROR,
  DB_TYPE_STRING,
  DB_TYPE_LIST,
  DB_TYPE_UINT,
  DB_TYPE_BOOL
} db_type_t;

typedef enum db_action_t
{
  DB_UNKNOWN_COMMAND,
  DB_SAVE,
  DB_START,
  DB_SET,
  DB_GET,
  DB_RENAME,
  DB_DEL,
  DB_LPUSH,
  DB_LPOP,
  DB_RPUSH,
  DB_RPOP,
  DB_LLEN,
  DB_LRANGE,
  DB_KEYS,
  DB_FLUSHALL,
  DB_INFO_DATASET_MEMORY,
  DB_SHUTDOWN
} db_action_t;

typedef int32_t db_int_t;
typedef uint32_t db_size_t;

#define DB_SIZE_MAX UINT32_MAX

typedef struct DLNode
{
  char *data;
  struct DLNode *prev;
  struct DLNode *next;
} DLNode;

typedef struct DLList
{
  DLNode *head;
  DLNode *tail;
  db_size_t length;
} DLList;

typedef struct DBArg
{
  db_type_t type;
  union
  {
    char *string;
    db_size_t size;
    db_int_t signed_int;
  } value;
  struct DBArg *next;
} DBArg;

typedef struct DBRequest
{
  db_action_t action;
  DBArg *arg_head;
  DBArg *arg_tail;
} DBRequest;

typedef struct DBReply
{
  bool ok;
  bool done;
  db_type_t type;
  union
  {
    char *string;
    DLList *list;
    db_size_t size;
    db_int_t signed_int;
    bool boolean;
  } value;
} DBReply;

#endif
