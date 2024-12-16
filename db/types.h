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
  DB_TYPE_BOOL,
  DB_TYPE_INT,
  DB_TYPE_UINT,
  DB_TYPE_DOUBLE,
  DB_TYPE_STRING,
  DB_TYPE_LIST,
  DB_TYPE_ZSET,
  DB_TYPE_HASH
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
typedef uint32_t db_uint_t;
typedef double db_double_t;

#define DB_SIZE_MAX UINT32_MAX

typedef struct DBObj DBObj;

typedef struct DBListNode
{
  struct DBListNode *prev;
  struct DBListNode *next;
  DBObj *data;
} DBListNode;

typedef struct DBList
{
  DBListNode *head;
  DBListNode *tail;
  db_uint_t length;
} DBList;

typedef struct DBHashEntry
{
  char *key;
  struct DBHashEntry *next;
  DBObj *data;
} DBHashEntry;

typedef struct DBHash
{
  db_uint_t size0;
  db_uint_t count0;
  DBHashEntry **buckets0;
  db_uint_t size1;
  db_uint_t count1;
  DBHashEntry **buckets1;
  // tables[0] is the main table, tables[1] is the rehash table
  // During rehashing, entries are first searched and deleted from tables[1], then from tables[0].
  // New entries are only written to tables[1] during rehashing.
  // After rehashing is complete, tables[0] is freed and tables[1] is moved to the main table.
  // -1 indicates no rehashing; otherwise, it's the current rehashing index
  // The occurrence of rehashing is determined by periodic tasks; when rehashing starts, rehashing_index will be the last index of the table size
  // Rehashing will be handled during periodic task execution and during db_insert_entry and db_get_entry.
  db_int_t rehashing_index;
} DBHash;

typedef struct DBZSet
{
} DBZSet;

typedef struct DBObj
{
  db_type_t type;
  union DBObjValue
  {
    bool bool_value;
    db_int_t int_value;
    db_uint_t uint_value;
    db_double_t double_value;
    void *_pointer;
    char *message;
    char *string;
    DBList *list;
    DBZSet *zset;
    DBHash *hash;
  } value;
} DBObj;

typedef struct DBRequest
{
  db_action_t action;
  DBList *args;
} DBRequest;

typedef struct DBReply
{
  bool done;
  DBObj *data;
} DBReply;

#endif
