#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/time.h>
#include <unistd.h>

#include "db/utils.h"
#include "db/api.h"

#define MAX_SEQUENCE 0xFFFF
#define USER_NS "user"
#define POST_NS "post"
#define TAG_NS "tag"
#define INDEX_NS "index"
#define NAME_FIELD_NAME "name"
#define TAGS_FIELD_NAME "tags"
#define ATAGS_FIELD_NAME "atags"
#define PTAGS_FIELD_NAME "ptags"

static char *generate_oid()
{
  static uint64_t last_timestamp = 0;
  static uint16_t sequence = 0;

  // Allocate memory to store the generated OID (16 characters + null terminator)
  char *oid = (char *)malloc(17);
  if (oid == NULL)
  {
    perror("Failed to allocate memory for OID");
    exit(EXIT_FAILURE);
  }

  // Get the current timestamp in milliseconds
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint64_t current_timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;

  // Reset sequence if timestamp changes
  if (current_timestamp != last_timestamp)
  {
    last_timestamp = current_timestamp;
    sequence = 1;
  }
  else
  {
    // Increment sequence
    sequence++;

    // If sequence exceeds max value, sleep for a short time
    if (sequence > MAX_SEQUENCE)
    {
      sequence = 1;
      usleep(1); // Sleep for 1 microsecond
      free(oid);
      return generate_oid();
    }
  }

  // Generate the OID
  snprintf(oid, 17, "%012llx%04x", (unsigned long long)last_timestamp, sequence);
  return oid;
}

// 提取 key 的 OID，回傳一個新的 char * 用完要記得 free
// 若找無，回傳 NULL。
static char *parse_oid(const char *query_key)
{
  if (!query_key)
    return NULL;

  const char *first_colon = strchr(query_key, ':');

  if (!first_colon)
    return NULL;

  const char *second_colon = strchr(first_colon + 1, ':');

  size_t oid_length = second_colon ? (size_t)(second_colon - first_colon - 1) : strlen(first_colon + 1);

  char *oid = (char *)malloc(oid_length + 1);

  if (!oid)
    EXIT_ON_MEMORY_ERROR();

  strncpy(oid, first_colon + 1, oid_length);
  oid[oid_length] = '\0';

  return oid;
}

static DBList *convert_to_ids(DBList *keys)
{
  DBListNode *curr = keys->head;
  while (curr)
  {
    char *id = parse_oid(curr->data->value.string);
    free(curr->data->value.string);
    curr->data->value.string = id;
    curr = curr->next;
  }
  return keys;
}

static char *create_query_key(const char *namespace, const char *id, const char *field_name)
{
  // 3 = 2 * strlen(":") + 1;
  const size_t key_length = strlen(namespace) + strlen(id) + (field_name ? (strlen(field_name) + 1) : 0) + 3;
  char *key = (char *)malloc(key_length);
  if (!key)
    EXIT_ON_MEMORY_ERROR();
  if (field_name)
    snprintf(key, key_length, "%s:%s:%s", namespace, id, field_name);
  else
    snprintf(key, key_length, "%s:%s", namespace, id);
  return key;
}

static void db_set_list(const char *key, DBList *list)
{
  dbapi_del(key);
  DBListNode *node = list->head;
  while (node)
  {
    dbapi_rpush(key, node->data->value.string);
    node = node->next;
  }
}

DBList *get_user_ids()
{
  char *pattern = create_query_key(USER_NS, "*", NULL);
  DBList *keys = dbapi_match_keys(pattern);
  free(pattern);
  return convert_to_ids(keys);
};

char *create_user_with_id_returned(const char *name, DBList *atags)
{
  char *oid = generate_oid(oid);

  char *query_key = create_query_key(USER_NS, oid, NAME_FIELD_NAME);
  dbapi_set(query_key, name);
  free(query_key);

  query_key = create_query_key(USER_NS, oid, ATAGS_FIELD_NAME);
  db_set_list(query_key, atags);
  free(query_key);

  return oid;
}

void create_user(const char *name, DBList *atags)
{
  free(create_user_with_id_returned(name, atags));
}

char *get_user_id_by_name(const char *name)
{
  DBList *user_ids = get_user_ids();
  DBListNode *curr = user_ids->head;
  while (curr)
  {
    const char *user_id = curr->data->value.string;
    char *query_key = create_query_key(USER_NS, user_id, NAME_FIELD_NAME);
    const char *user_name = dbapi_get(query_key);
    free(query_key);
    if (user_name && strcmp(user_name, name) == 0)
    {
      char *oid = dbutil_strdup(user_id);
      free_dblist(user_ids);
      return oid;
    }
    curr = curr->next;
  }
  free_dblist(user_ids);
  return NULL;
}

void delete_user(const char *oid)
{
  if (!oid)
    return;

  char *query_key = create_query_key(USER_NS, oid, NAME_FIELD_NAME);
  dbapi_del(query_key);
  free(query_key);

  query_key = create_query_key(USER_NS, oid, ATAGS_FIELD_NAME);
  dbapi_del(query_key);
  free(query_key);

  query_key = create_query_key(USER_NS, oid, PTAGS_FIELD_NAME);
  dbapi_del(query_key);
  free(query_key);
}

DBList *get_post_ids()
{
  char *pattern = create_query_key(POST_NS, "*", NULL);
  DBList *keys = dbapi_match_keys(pattern);
  free(pattern);
  return convert_to_ids(keys);
}

char *create_post_with_id_returned(DBList *tags)
{
  char *oid = generate_oid(oid);

  char *query_key = create_query_key(POST_NS, oid, TAGS_FIELD_NAME);
  db_set_list(query_key, tags);
  free(query_key);

  return oid;
}

void create_post(DBList *tags)
{
  free(create_post_with_id_returned(tags));
}

void create_post_indexes()
{
  DBList *tags = get_tag_ids();
  DBListNode *curr = tags->head;
  while (curr)
  {
    char *tag_id = curr->data->value.string;
    char *query_key = create_query_key(INDEX_NS, tag_id, NULL);
    DBList *posts = get_posts_by_tag(tag_id, -1, false);
    db_set_list(query_key, posts);
    free(query_key);
    free_dblist(posts);
    curr = curr->next;
  }
  free_dblist(tags);
}

void delete_posts()
{
  char *pattern = create_query_key(POST_NS, "*", NULL);
  DBList *keys = dbapi_match_keys(pattern);
  free(pattern);
  DBListNode *curr = keys->head;
  while (curr)
    dbapi_del(curr->data->value.string), curr = curr->next;
  free_dblist(keys);

  pattern = create_query_key(INDEX_NS, "*", NULL);
  keys = dbapi_match_keys(pattern);
  free(pattern);
  curr = keys->head;
  while (curr)
    dbapi_del(curr->data->value.string), curr = curr->next;
  free_dblist(keys);
}

DBList *get_post_tags(const char *post_id)
{
  char *query_key = create_query_key(POST_NS, post_id, TAGS_FIELD_NAME);
  DBList *post_tags = dbapi_lrange(query_key, 0, DB_UINT_MAX);
  free(query_key);

  return post_tags;
}

DBList *get_posts_by_tag(const char *tag_id, size_t limit, const bool by_index)
{
  if (by_index)
  {
    char *query_key = create_query_key(INDEX_NS, tag_id, NULL);
    DBList *posts = dbapi_lrange(query_key, 0, limit);
    free(query_key);
    return posts;
  }

  DBList *filtered_post_ids = create_dblist();
  DBList *post_ids = get_post_ids();
  DBListNode *post_id_node = post_ids->head;

  while (post_id_node && filtered_post_ids->length < limit)
  {
    const char *post_id = post_id_node->data->value.string;
    DBList *post_tags = get_post_tags(post_id);
    DBListNode *post_tag_node = post_tags->head;
    while (post_tag_node)
    {
      const char *post_tag_id = post_tag_node->data->value.string;
      if (strcmp(post_tag_id, tag_id) == 0)
      {
        rpush(filtered_post_ids, create_dblistnode_with_string(post_id));
        break;
      }
      post_tag_node = post_tag_node->next;
    }
    free_dblist(post_tags);
    post_id_node = post_id_node->next;
  }

  free_dblist(post_ids);
  return filtered_post_ids;
}

DBList *get_tag_ids()
{
  char *pattern = create_query_key(TAG_NS, "*", NULL);
  DBList *keys = dbapi_match_keys(pattern);
  free(pattern);

  return convert_to_ids(keys);
}

char *create_tag_with_id_returned(const char *name)
{
  char *oid = generate_oid();

  char *query_key = create_query_key(TAG_NS, oid, NAME_FIELD_NAME);
  dbapi_set(query_key, name);
  free(query_key);

  return oid;
}

void create_tag(const char *name)
{
  free(create_tag_with_id_returned(name));
}

DBList *get_user_atags(const char *user_id)
{
  char *query_key = create_query_key(USER_NS, user_id, ATAGS_FIELD_NAME);
  DBList *user_atags = dbapi_lrange(query_key, 0, DB_UINT_MAX);
  free(query_key);

  return user_atags;
}

db_bool_t set_user_atags(const char *user_id, DBList *tags)
{
  char *query_key = create_query_key(USER_NS, user_id, ATAGS_FIELD_NAME);
  db_set_list(query_key, tags);
  free(query_key);

  return true; // 成功更新 atags，返回 true
}

DBList *get_user_ptags(const char *user_id)
{
  char *query_key = create_query_key(USER_NS, user_id, PTAGS_FIELD_NAME);
  DBList *user_ptags = dbapi_lrange(query_key, 0, DB_UINT_MAX);
  free(query_key);

  return user_ptags;
}

db_bool_t set_user_ptags(const char *user_id, DBList *tags)
{
  char *query_key = create_query_key(USER_NS, user_id, PTAGS_FIELD_NAME);
  db_set_list(query_key, tags);
  free(query_key);

  return true; // 成功更新 s，返回 true
}

void start_db(void)
{
  dbapi_start_server();
}

void save_db(void)
{
  dbapi_save();
}

void flush_all(void)
{
  dbapi_flushall();
  save_db();
}
