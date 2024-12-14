#include <string.h>
#include <time.h>
#include <malloc.h>
#include <threads.h>

#include "deps/cJSON.h"
#include "utils.h"
#include "doubly_linked_list.h"
#include "interaction.h"
#include "core.h"

typedef struct DBTask
{
  clock_t created_at;
  DBRequest *request;
  DBReply *reply;
  struct DBTask *next;
} DBTask;

typedef struct CoreHTEntry
{
  // Key for the entry
  char *key;
  // Type of the stored value (e.g., string or list)
  db_type_t type;
  // Value for the entry
  union DBValue
  {
    char *string;
    DLList *list;
  } value;
  // Pointer to the next entry in case of a hash collision
  struct CoreHTEntry *next;
} CoreHTEntry;

typedef struct CoreHT
{
  // Number of slots in the hash table
  db_size_t size;
  // Current number of entries in the hash table
  db_size_t count;
  // Array of entries
  CoreHTEntry **entries;
} CoreHT;

static inline void core_lock_init();

static int core_worker();

// Computes the MurmurHash2 hash of a key
static db_size_t murmurhash2(const void *key, db_size_t len);

// Executed during each low-level operation and periodic task to maintain the hash table size
static void core_maintenance();

// Checks if rehashing is needed and performs a rehash step if required
// Returns true if additional rehash steps are required
static bool core_rehash_step();

// Creates a new hash table with the specified size
static CoreHT *core_create_table(db_size_t size);

// Frees the memory allocated for a hash table
static void core_free_table(CoreHT *table);

// Creates a new entry with the specified key and type; assigns value directly
static CoreHTEntry *create_entry(char *key, db_type_t type, void *value);

// Frees the memory allocated for a hash table entry
static void core_free_entry(CoreHTEntry *entry);

static void core_set_entry_value(CoreHTEntry *entry, db_type_t type, void *value);

// Retrieves an entry by key; returns NULL if not found
static CoreHTEntry *core_get_entry(const char *key);

// Adds an entry to the hash table
static CoreHTEntry *core_add_entry(CoreHTEntry *entry);

// Removes an entry by key; returns NULL if not found
static CoreHTEntry *core_remove_entry(const char *key);

// Retrieves a string by key;
static const char const *core_retrieve_string(const char *key);

// Retrieves a list by key;
static DLList *core_retrieve_list(const char *key, const bool create_new_if_not_found);

// Seed for the hash function, affecting hash distribution
static db_size_t hash_seed = 0;

// File path for database persistence
static char *persistence_filepath = NULL;

// tables[0] is the main table, tables[1] is the rehash table
// During rehashing, entries are first searched and deleted from tables[1], then from tables[0].
// New entries are only written to tables[1] during rehashing.
// After rehashing is complete, tables[0] is freed and tables[1] is moved to the main table.
static CoreHT *tables[2] = {NULL, NULL};

// -1 indicates no rehashing; otherwise, it's the current rehashing index
// The occurrence of rehashing is determined by periodic tasks; when rehashing starts, rehashing_index will be the last index of the table size
// Rehashing will be handled during periodic task execution and during db_insert_entry and db_get_entry.
static db_int_t rehashing_index = -1;

static bool is_running = false;
static mtx_t *lock = NULL;
static thrd_t core_worker_thread = -1;

static DBTask *task_queue_head = NULL;
static DBTask *task_queue_tail = NULL;

static inline void core_lock_init()
{
  if (!lock)
  {
    lock = (mtx_t *)calloc(1, sizeof(mtx_t));
    if (!lock)
      EXIT_ON_MEMORY_ERROR();
    mtx_init(lock, mtx_plain);
  }
}

int core_lock()
{
  core_lock_init();
  return mtx_lock(lock);
}

int core_unlock()
{
  core_lock_init();
  return mtx_unlock(lock);
}

bool core_trylock_is_success()
{
  core_lock_init();
  return mtx_trylock(lock) == thrd_success;
}

void db_start()
{
  if (is_running)
    return;

  is_running = true;

  db_flushall(NULL, NULL);

  db_config_hash_seed(hash_seed);
  if (!persistence_filepath)
    db_config_persistence_filepath(DEFAULT_PERSISTENCE_FILE);

  // load data
  FILE *file = fopen(persistence_filepath, "r");
  if (file)
  {
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer)
      EXIT_ON_MEMORY_ERROR();
    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';
    fclose(file);

    char *key = NULL;
    DLList *list;
    cJSON *cjson_cursor = cJSON_Parse(buffer);
    cJSON *cjson_array_cursor = NULL;
    free(buffer);

    if (cjson_cursor)
      cjson_cursor = cjson_cursor->child;

    while (cjson_cursor)
    {
      key = cjson_cursor->string;

      if (!key)
      {
        cjson_cursor = cjson_cursor->next;
        continue;
      }

      core_maintenance();

      if (cJSON_IsString(cjson_cursor))
      {
        core_add_entry(create_entry(key, DB_TYPE_STRING, dbutil_strdup(cJSON_GetStringValue(cjson_cursor))));
      }

      else if (cJSON_IsArray(cjson_cursor))
      {
        cjson_array_cursor = cjson_cursor->child;

        list = create_list();
        while (cjson_array_cursor)
        {
          if (cJSON_IsString(cjson_array_cursor))
          {
            rpush(list, create_list_node(cJSON_GetStringValue(cjson_array_cursor)));
          }

          cjson_array_cursor = cjson_array_cursor->next;
        }
        core_add_entry(create_entry(key, DB_TYPE_LIST, list));
      }

      cjson_cursor = cjson_cursor->next;
    }
  }

  thrd_create(&core_worker_thread, core_worker, NULL);
}

bool db_is_running()
{
  return is_running;
}

void db_config_hash_seed(db_size_t _hash_seed)
{
  hash_seed = _hash_seed ? _hash_seed : (db_size_t)time(NULL);
}

void db_config_persistence_filepath(const char *_persistence_filepath)
{
  free(persistence_filepath);
  persistence_filepath = dbutil_strdup(_persistence_filepath);
}

DBReply *db_handle_request(DBRequest *request)
{
  DBReply *reply = (DBReply *)calloc(1, sizeof(DBReply));
  if (!reply)
    EXIT_ON_MEMORY_ERROR();

  reply->ok = false;

  if (!is_running)
  {
    reply->ok = false;
    reply->type = DB_TYPE_ERROR;
    reply->value.string = dbutil_strdup(DB_ERR_DB_IS_CLOSED);
    return reply;
  }

  DBTask *task = (DBTask *)malloc(sizeof(DBTask));
  if (!task)
    EXIT_ON_MEMORY_ERROR();

  task->created_at = clock();
  task->request = request;
  task->reply = reply;
  task->next = NULL;

  if (!task_queue_head)
  {
    task_queue_head = task;
    task_queue_tail = task;
  }
  else
  {
    task_queue_tail->next = task;
    task_queue_tail = task;
  }

  return reply;
}

static int core_worker()
{
  clock_t t0;
  DBRequest *request;
  DBReply *reply;
  DBArg *arg1, *arg2, *arg3;
  // Calculate the sleep increment to reach 1 second over 5 minutes, in nanoseconds.
  const long sleep_increment_ns = NANOSECONDS_PER_SECOND / (5 * 60 * 1000);
  clock_t idle_start_time = 0;
  long sleep_duration_ns = 0;

  while (is_running)
  {
    if (!core_trylock_is_success())
      continue;

    if (task_queue_head)
    {
      if (task_queue_head->request->action != DB_INFO_DATASET_MEMORY)
      {
        idle_start_time = 0;
        sleep_duration_ns = 0;
      }
      do
      {
        core_maintenance();
        request = task_queue_head->request;
        reply = task_queue_head->reply;
        reply->ok = true;
        switch (request->action)
        {
        case DB_GET:
          db_get(request, reply);
          break;
        case DB_SET:
          db_set(request, reply);
          break;
        case DB_RENAME:
          db_rename(request, reply);
          break;
        case DB_DEL:
          db_del(request, reply);
          break;
        case DB_LPUSH:
          db_lpush(request, reply);
          break;
        case DB_LPOP:
          db_lpop(request, reply);
          break;
        case DB_RPUSH:
          db_rpush(request, reply);
          break;
        case DB_RPOP:
          db_rpop(request, reply);
          break;
        case DB_LLEN:
          db_llen(request, reply);
          break;
        case DB_LRANGE:
          db_lrange(request, reply);
          break;
        case DB_KEYS:
          db_keys(request, reply);
          break;
        case DB_FLUSHALL:
          db_flushall(request, reply);
          break;
        case DB_INFO_DATASET_MEMORY:
          db_get_dataset_memory_usage(request, reply);
          break;
        case DB_SAVE:
          db_save(request, reply);
          break;
        case DB_SHUTDOWN:
          db_shutdown(request, reply);
          break;
        default:
          reply->ok = false;
          reply->type = DB_TYPE_ERROR;
          reply->value.string = dbutil_strdup(DB_ERR_UNKNOWN_COMMAND);
          break;
        }
        reply->done = true;
        free(task_queue_head);
        task_queue_head = task_queue_head->next;
        if (!task_queue_head)
          task_queue_tail = NULL;
      } while (task_queue_head);
      core_unlock();
    }
    else
    {
      core_maintenance();
      core_unlock();
      if (!idle_start_time)
      {
        idle_start_time = clock();
      }
      // If the idle time exceeds 100ms,
      // a progressively increasing sleep period begins.
      if ((clock() - idle_start_time) * 1000 / CLOCKS_PER_SEC > 100)
      {
        if (sleep_duration_ns < NANOSECONDS_PER_SECOND)
          sleep_duration_ns += sleep_increment_ns;
        thrd_sleep(&(struct timespec){.tv_sec = 0, .tv_nsec = sleep_duration_ns}, NULL);
      }
    }
  }

  return 0;
}

static db_size_t murmurhash2(const void *key, db_size_t len)
{
  const db_size_t m = 0x5bd1e995;
  const int r = 24;
  db_size_t h = hash_seed ^ len;

  const unsigned char *data = (const unsigned char *)key;

  while (len >= 4)
  {
    db_size_t k = *(db_size_t *)data;
    k *= m, k ^= k >> r, k *= m;
    h *= m, h ^= k;
    data += 4, len -= 4;
  }

  switch (len)
  {
  case 3:
    h ^= data[2] << 16;
  case 2:
    h ^= data[1] << 8;
  case 1:
    h ^= data[0];
    h *= m;
  }

  h ^= h >> 13, h *= m, h ^= h >> 15;

  return h;
}

void db_get_dataset_memory_usage(DBRequest *request, DBReply *reply)
{
  size_t size = 2 * sizeof(CoreHT *);
  CoreHTEntry *entry;
  DLNode *dllnode;

  for (int j = 0; j < 2; ++j)
  {
    if (!tables[j])
      continue;
    size += malloc_usable_size(tables[j]);
    size += malloc_usable_size(tables[j]->entries);
    for (db_size_t i = 0; i < tables[j]->size; ++i)
    {
      entry = tables[j]->entries[i];
      while (entry)
      {
        size += malloc_usable_size(entry);
        size += malloc_usable_size(entry->key);
        switch (entry->type)
        {
        case DB_TYPE_STRING:
          size += malloc_usable_size(entry->value.string);
          break;
        case DB_TYPE_LIST:
          size += malloc_usable_size(entry->value.list);
          dllnode = entry->value.list->head;
          while (dllnode)
          {
            size += malloc_usable_size(dllnode);
            size += malloc_usable_size(dllnode->data);
            dllnode = dllnode->next;
          }
          break;
        default:
          break;
        }
        entry = entry->next;
      }
    }
  }

  reply->type = DB_TYPE_UINT;
  reply->value.size = size;
}

static void core_maintenance()
{
  if (!tables[1])
  {
    if (tables[0]->count > LOAD_FACTOR_EXPAND * tables[0]->size)
    {
      rehashing_index = tables[0]->size - 1;
      tables[1] = core_create_table(tables[0]->size * 2);
    }
    else if (tables[0]->size > INITIAL_TABLE_SIZE && tables[0]->count < LOAD_FACTOR_SHRINK * tables[0]->size)
    {
      rehashing_index = tables[0]->size - 1;
      tables[1] = core_create_table(tables[0]->size / 2);
    }
  }
  else
    core_rehash_step();
}

static bool core_rehash_step()
{
  if (!tables[1])
    return false; // Not rehashing

  // Move entries from tables[0] to tables[1]
  db_size_t index;
  CoreHTEntry *curr_entry = tables[0]->entries[rehashing_index];
  CoreHTEntry *next_entry;

  while (curr_entry)
  {
    next_entry = curr_entry->next;
    index = murmurhash2(curr_entry->key, strlen(curr_entry->key)) % tables[1]->size;
    curr_entry->next = tables[1]->entries[index];
    tables[1]->entries[index] = curr_entry;
    ++tables[1]->count;
    --tables[0]->count;
    curr_entry = next_entry;
  }

  tables[0]->entries[rehashing_index] = NULL;
  --rehashing_index;

  if (rehashing_index == (int32_t)(-1))
  {
    core_free_table(tables[0]);
    tables[0] = tables[1];
    tables[1] = NULL; // Clear the rehash table
    return false;
  }

  return true;
}

static CoreHT *core_create_table(db_size_t size)
{
  CoreHT *table = (CoreHT *)malloc(sizeof(CoreHT));
  if (!table)
    EXIT_ON_MEMORY_ERROR();

  table->size = size;
  table->count = 0;
  table->entries = (CoreHTEntry **)calloc(size, sizeof(CoreHTEntry *));

  if (!table->entries)
    EXIT_ON_MEMORY_ERROR();

  return table;
}

static void core_free_table(CoreHT *table)
{
  if (!table)
    return;
  db_size_t i;

  CoreHTEntry *curr_entry, *next_entry;
  for (i = 0; i < table->size; ++i)
  {
    curr_entry = table->entries[i];
    next_entry = NULL;
    while (curr_entry)
    {
      next_entry = curr_entry->next;
      core_free_entry(curr_entry);
      curr_entry = next_entry;
    }
  }

  free(table->entries);
  free(table);
}

static CoreHTEntry *create_entry(char *key, db_type_t type, void *value)
{
  if (!key || !value)
    return NULL;

  CoreHTEntry *entry = (CoreHTEntry *)malloc(sizeof(CoreHTEntry));

  if (!entry)
    EXIT_ON_MEMORY_ERROR();

  entry->key = key;
  entry->next = NULL;

  core_set_entry_value(entry, type, value);

  return entry;
}

static void core_free_entry(CoreHTEntry *entry)
{
  if (!entry)
    return;

  if (entry->key)
    free(entry->key);

  core_set_entry_value(entry, DB_TYPE_NULL, NULL);

  free(entry);
}

static void core_set_entry_value(CoreHTEntry *entry, db_type_t type, void *value)
{
  if (!entry)
    return;

  if (entry->type != type)
  {
    switch (entry->type)
    {
    case DB_TYPE_STRING:
      free(entry->value.string);
      entry->value.string = NULL;
      break;
    case DB_TYPE_LIST:
      free_list(entry->value.list);
      entry->value.list = NULL;
      break;
    default:
      break;
    }
    entry->type = type;
  }

  if (!value)
    return;

  switch (type)
  {
  case DB_TYPE_STRING:
    entry->value.string = value;
    break;
  case DB_TYPE_LIST:
    entry->value.list = value;
    break;
  default:
    break;
  }
}

static CoreHTEntry *core_get_entry(const char *key)
{
  if (!key)
    return NULL;

  CoreHTEntry *entry;

  if (tables[1])
  {
    entry = tables[1]->entries[murmurhash2(key, strlen(key)) % tables[1]->size];
    while (entry)
    {
      if (strcmp(entry->key, key) == 0)
        return entry;
      entry = entry->next;
    }
  }

  entry = tables[0]->entries[murmurhash2(key, strlen(key)) % tables[0]->size];
  while (entry)
  {
    if (strcmp(entry->key, key) == 0)
      return entry;
    entry = entry->next;
  }

  return NULL;
}

static CoreHTEntry *core_add_entry(CoreHTEntry *entry)
{
  if (!entry)
    return NULL;

  db_size_t index;

  if (tables[1])
  {
    index = murmurhash2(entry->key, strlen(entry->key)) % tables[1]->size;
    entry->next = tables[1]->entries[index];
    tables[1]->entries[index] = entry;
    ++tables[1]->count;
    return entry;
  }

  index = murmurhash2(entry->key, strlen(entry->key)) % tables[0]->size;
  entry->next = tables[0]->entries[index];
  tables[0]->entries[index] = entry;
  ++tables[0]->count;
  return entry;
}

static CoreHTEntry *core_remove_entry(const char *key)
{
  if (!key)
    return NULL;

  CoreHTEntry *curr_entry, *prev_entry = NULL;
  db_size_t index;

  if (tables[1])
  {
    index = murmurhash2(key, strlen(key)) % tables[1]->size;
    curr_entry = tables[1]->entries[index];
    while (curr_entry)
    {
      if (strcmp(curr_entry->key, key) == 0)
      {
        if (prev_entry)
          prev_entry->next = curr_entry->next;
        else
          tables[1]->entries[index] = curr_entry->next;
        --tables[1]->count;
        return curr_entry;
      }
      prev_entry = curr_entry;
      curr_entry = curr_entry->next;
    }
  }

  index = murmurhash2(key, strlen(key)) % tables[0]->size;
  curr_entry = tables[0]->entries[index];
  prev_entry = NULL;
  while (curr_entry)
  {
    if (strcmp(curr_entry->key, key) == 0)
    {
      if (prev_entry)
        prev_entry->next = curr_entry->next;
      else
        tables[0]->entries[index] = curr_entry->next;
      --tables[0]->count;
      return curr_entry;
    }
    prev_entry = curr_entry;
    curr_entry = curr_entry->next;
  }

  return NULL;
}

static const char const *core_retrieve_string(const char *key)
{
  if (!key)
    return NULL;

  CoreHTEntry *entry = core_get_entry(key);

  if (entry && entry->type == DB_TYPE_STRING)
  {
    return entry->value.string;
  }

  return NULL;
}

static DLList *core_retrieve_list(const char *key, const bool create_new_if_not_found)
{
  if (!key)
    return NULL;

  CoreHTEntry *entry = core_get_entry(key);

  if (entry)
  {
    return entry->type == DB_TYPE_LIST ? entry->value.list : NULL;
  }

  if (create_new_if_not_found)
  {
    DLList *list = create_list();
    core_add_entry(create_entry(dbutil_strdup(key), DB_TYPE_LIST, list));

    return list;
  }

  return NULL;
}

void db_get(DBRequest *request, DBReply *reply)
{
  DBArg *arg1 = request->arg_head;

  if (!arg1)
  {
    reply_error(reply, DB_ERR_ARG_ERROR);
    return;
  }

  const char const *value = core_retrieve_string(arg1->value.string);

  if (value)
  {
    // Return the string value
    reply->type = DB_TYPE_STRING;
    reply->value.string = dbutil_strdup(value);
  }
  else
  {
    // Not found
    reply->type = DB_TYPE_NULL;
  }
}

void db_set(DBRequest *request, DBReply *reply)
{
  DBArg *arg1 = request->arg_head;
  DBArg *arg2 = arg1 ? arg1->next : NULL;

  if (!arg1 || !arg2)
  {
    reply_error(reply, DB_ERR_ARG_ERROR);
    return;
  }

  CoreHTEntry *entry = core_get_entry(arg1->value.string);

  if (entry)
    core_set_entry_value(entry, DB_TYPE_STRING, dbutil_strdup(arg2->value.string));
  else
    core_add_entry(create_entry(dbutil_strdup(arg1->value.string), DB_TYPE_STRING, dbutil_strdup(arg2->value.string)));

  reply->type = DB_TYPE_BOOL;
  reply->value.boolean = true;
}

void db_rename(DBRequest *request, DBReply *reply)
{
  DBArg *arg1 = request->arg_head;
  DBArg *arg2 = arg1 ? arg1->next : NULL;

  if (!arg1 || !arg2)
  {
    reply_error(reply, DB_ERR_ARG_ERROR);
    return;
  }

  CoreHTEntry *entry = core_remove_entry(arg1->value.string);

  if (!entry)
  {
    reply_error(reply, DB_ERR_NONEXISTENT_KEY);
    return;
  }

  free(entry->key);
  entry->key = dbutil_strdup(arg2->value.string);
  core_add_entry(entry);

  reply->type = DB_TYPE_BOOL;
  reply->value.boolean = true;
}

void db_del(DBRequest *request, DBReply *reply)
{
  DBArg *arg = request->arg_head;

  if (!arg)
  {
    reply_error(reply, DB_ERR_ARG_ERROR);
    return;
  }

  CoreHTEntry *entry;
  db_size_t deleted_count = 0;

  while (arg)
  {
    entry = core_remove_entry(arg->value.string);
    if (entry)
      core_free_entry(entry), ++deleted_count;
    arg = arg->next;
  }

  reply->type = DB_TYPE_UINT;
  reply->value.size = deleted_count;
}

void db_lpush(DBRequest *request, DBReply *reply)
{
  DBArg *arg1 = request->arg_head;
  DBArg *arg2 = arg1 ? arg1->next : NULL;

  if (!arg1 || !arg2)
  {
    reply_error(reply, DB_ERR_ARG_ERROR);
    return;
  }

  DLList *list = core_retrieve_list(arg1->value.string, true);

  if (!list)
  {
    reply_error(reply, DB_ERR_WRONGTYPE);
    return;
  }

  while (arg2)
  {
    lpush(list, create_list_node(arg2->value.string));
    arg2 = arg2->next;
  }

  reply->type = DB_TYPE_UINT;
  reply->value.size = list->length;
}

void db_lpop(DBRequest *request, DBReply *reply)
{
  DBArg *arg1 = request->arg_head;
  DBArg *arg2 = arg1 ? arg1->next ? arg_string_to_uint(arg1->next) : add_arg_uint(request, 1) : NULL;

  if (!arg1 || !arg2)
  {
    reply_error(reply, DB_ERR_ARG_ERROR);
    return;
  }

  DLList *list = core_retrieve_list(arg1->value.string, false);

  if (!list)
  {
    reply->type = DB_TYPE_NULL;
    return;
  }

  db_size_t count = arg2->value.size;

  if (count == 1)
  {
    reply->type = DB_TYPE_STRING;
    reply->value.string = extract_list_node(lpop(list));
  }
  else if (count)
  {
    reply->type = DB_TYPE_LIST;
    reply->value.list = lpop_n(list, count);
  }
}

void db_rpush(DBRequest *request, DBReply *reply)
{
  DBArg *arg1 = request->arg_head;
  DBArg *arg2 = arg1 ? arg1->next : NULL;

  if (!arg1 || !arg2)
  {
    reply_error(reply, DB_ERR_ARG_ERROR);
    return;
  }

  DLList *list = core_retrieve_list(arg1->value.string, true);

  if (!list)
  {
    reply_error(reply, DB_ERR_WRONGTYPE);
    return;
  }

  while (arg2)
  {
    rpush(list, create_list_node(arg2->value.string));
    arg2 = arg2->next;
  }

  reply->type = DB_TYPE_UINT;
  reply->value.size = list->length;
}

void db_rpop(DBRequest *request, DBReply *reply)
{
  DBArg *arg1 = request->arg_head;
  DBArg *arg2 = arg1 ? arg1->next ? arg_string_to_uint(arg1->next) : add_arg_uint(request, 1) : NULL;

  if (!arg1 || !arg2)
  {
    reply_error(reply, DB_ERR_ARG_ERROR);
    return;
  }

  DLList *list = core_retrieve_list(arg1->value.string, false);

  if (!list)
  {
    reply->type = DB_TYPE_NULL;
    return;
  }

  db_size_t count = arg2->value.size;

  if (count == 1)
  {
    reply->type = DB_TYPE_STRING;
    reply->value.string = extract_list_node(rpop(list));
  }
  else if (count)
  {
    reply->type = DB_TYPE_LIST;
    reply->value.list = rpop_n(list, count);
  }
}

void db_llen(DBRequest *request, DBReply *reply)
{
  DBArg *arg1 = request->arg_head;

  if (!arg1)
  {
    reply_error(reply, DB_ERR_ARG_ERROR);
    return;
  }

  const DLList const *list = core_retrieve_list(arg1->value.string, false);

  reply->type = DB_TYPE_UINT;
  reply->value.size = list ? list->length : 0;
}

void db_lrange(DBRequest *request, DBReply *reply)
{
  DBArg *arg1 = request->arg_head;

  if (!arg1)
  {
    reply_error(reply, DB_ERR_ARG_ERROR);
    return;
  }

  db_size_t start = arg1->next ? arg_string_to_uint(arg1->next)->value.size : 0;
  db_size_t stop = arg1->next ? arg1->next->next ? arg_string_to_uint(arg1->next->next)->value.size : DB_SIZE_MAX : DB_SIZE_MAX;
  const DLList const *list = core_retrieve_list(arg1->value.string, false);

  reply->type = DB_TYPE_LIST;
  reply->value.list = lrange(list, start, stop);

  if (!reply->value.list)
  {
    reply_error(reply, DB_ERR_WRONGTYPE);
    return;
  }
}

void db_keys(DBRequest *request, DBReply *reply)
{
  CoreHTEntry *entry;
  db_size_t r;
  DLList *reply_list = create_list();

  reply->type = DB_TYPE_LIST;
  reply->value.list = reply_list;

  for (db_size_t t = 0; t < 2; ++t)
  {
    if (!tables[t])
      continue;
    for (r = 0; r < tables[t]->size; ++r)
    {
      entry = tables[t]->entries[r];
      while (entry)
      {
        rpush(reply_list, create_list_node(entry->key));
        entry = entry->next;
      }
    }
  }
}

void db_shutdown(DBRequest *request, DBReply *reply)
{
  reply->type = DB_TYPE_BOOL;
  if (!is_running)
  {
    reply->value.boolean = true;
    return;
  }

  is_running = false;
  thrd_join(core_worker_thread, NULL);

  db_save(request, reply);

  rehashing_index = -1;
  core_free_table(tables[0]);
  core_free_table(tables[1]);

  reply->value.boolean = true;
}

void db_save(DBRequest *request, DBReply *reply)
{
  reply->type = DB_TYPE_BOOL;

  if (!persistence_filepath)
  {
    reply->value.boolean = false;
    return;
  }

  cJSON *root = cJSON_CreateObject();
  CoreHTEntry *entry;
  DLNode *dllnode;
  cJSON *cjson_list;

  for (int j = 0; j < 2; ++j)
  {
    if (!tables[j])
      continue;

    for (db_size_t i = 0; i < tables[j]->size; ++i)
    {
      entry = tables[j]->entries[i];
      while (entry)
      {
        switch (entry->type)
        {
        case DB_TYPE_STRING:
          cJSON_AddItemToObject(root, entry->key, cJSON_CreateString(entry->value.string));
          break;
        case DB_TYPE_LIST:
          cjson_list = cJSON_CreateArray();
          dllnode = entry->value.list->head;
          while (dllnode)
          {
            cJSON_AddItemToArray(cjson_list, cJSON_CreateString(dllnode->data));
            dllnode = dllnode->next;
          }
          cJSON_AddItemToObject(root, entry->key, cjson_list);
          cjson_list = NULL;
          dllnode = NULL;
          break;
        default:
          break;
        }
        entry = entry->next;
      }
    }
  }

  FILE *file = fopen(persistence_filepath, "w");
  if (!file)
  {
    perror("Failed to open file while saving.");
    reply->value.boolean = false;
    return;
  }

  char *json_string = cJSON_PrintUnformatted(root);
  fputs(json_string, file);
  fclose(file);
  free(json_string);
  cJSON_Delete(root);

  reply->value.boolean = true;
}

void db_flushall(DBRequest *request, DBReply *reply)
{
  if (reply)
  {
    reply->type = DB_TYPE_BOOL;
    reply->value.boolean = true;
  }

  core_free_table(tables[0]);
  core_free_table(tables[1]);

  tables[0] = core_create_table(INITIAL_TABLE_SIZE);
  tables[1] = NULL;

  rehashing_index = -1;
}
