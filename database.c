// database.c
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <hiredis/hiredis.h>
#include <ctype.h>

#include "db/utils.h"
#include "db/api.h"

#define USER_NS "user"
#define POST_NS "post"
#define TAG_NS "tag"
#define USER_NS_PREFIX "user:"
#define POST_NS_PREFIX "post:"
#define TAG_NS_PREFIX "tag:"
#define NAME_FIELD_NAME "name"
#define TAGS_FIELD_NAME "tags"
#define NAME_FIELD_SUFFIX ":name"
#define TAGS_FIELD_SUFFIX ":tags"
#define ATAGS_FIELD_NAME "atags"
#define PTAGS_FIELD_NAME "ptags"

static bool starts_with(const char *str, const char *prefix)
{
  size_t prefix_len = strlen(prefix);
  return strncmp(str, prefix, prefix_len) == 0;
}

static bool ends_with(const char *str, const char *suffix)
{
  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);
  if (suffix_len > str_len)
    return false;
  return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

// 工具函數：檢查鍵的合法性
static bool is_valid_key(const char *key)
{
  const char invalid_chars[] = "*:[]?\\";
  while (*key)
  {
    if (strchr(invalid_chars, *key))
    {
      return false;
    }
    key++;
  }
  return true;
}

// 工具函數：生成唯一 OID
static char *generate_oid()
{
  char *oid = (char *)malloc(13);
  if (!oid)
    EXIT_ON_MEMORY_ERROR();
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  int timestamp = (int)time(NULL);
  sprintf(oid, "%06X", timestamp); // 前 6 位為時間戳
  for (int i = 0; i < 6; i++)
  {
    oid[6 + i] = charset[rand() % (sizeof(charset) - 1)];
  }
  oid[12] = '\0';
  return dbutil_strdup(oid);
}

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

static DBList *get_namespace_ids(const char *ns_prefix, const char *suffix)
{
  DBList *list = dbapi_keys();
  DBList *filtered = create_dblist();
  DBListNode *curr = list->head;

  while (curr)
  {
    if (!dbobj_is_string(curr->data))
      continue;
    const char *key = curr->data->value.string;
    if (starts_with(key, ns_prefix) && ends_with(key, suffix))
    {
      char *oid = parse_oid(key);
      rpush(filtered, create_dblistnode_with_string(oid));
      free(oid);
    }
    curr = curr->next;
  }

  free_dblist(list);

  return filtered;
};

static char *create_query_key(const char *namespace, const char *id, const char *field_name)
{
  // 3 = 2 * strlen(":") + 1;
  const size_t key_length = strlen(namespace) + strlen(id) + strlen(field_name) + 3;
  char *key = (char *)malloc(key_length);
  if (!key)
    EXIT_ON_MEMORY_ERROR();
  snprintf(key, key_length, "%s:%s:%s", namespace, id, field_name);
  return key;
}

static void util_dbapi_set_list(const char *key, DBList *list)
{
  dbapi_del(key);
  DBListNode *node = list->head;
  while (node)
  {
    dbapi_rpush(ATAGS_FIELD_NAME, node->data->value.string);
    node = node->next;
  }
}

DBList *get_user_ids()
{
  return get_namespace_ids(USER_NS_PREFIX, NAME_FIELD_SUFFIX);
};

char *create_user(const char *name, DBList *a_tags)
{
  char *oid = generate_oid(oid);

  char *query_key = create_query_key(USER_NS, oid, NAME_FIELD_NAME);
  dbapi_set(query_key, name);
  free(query_key);

  query_key = create_query_key(USER_NS, oid, ATAGS_FIELD_NAME);
  util_dbapi_set_list(query_key, a_tags);
  free(query_key);

  return oid;
}

DBList *get_post_ids()
{
  return get_namespace_ids(POST_NS_PREFIX, TAGS_FIELD_SUFFIX);
}

char *create_post(DBList *tags)
{
  char *oid = generate_oid(oid);

  char *query_key = create_query_key(POST_NS, oid, TAGS_FIELD_NAME);
  util_dbapi_set_list(query_key, tags);
  free(query_key);

  return oid;
}

DBList *get_post_tags(const char *post_id)
{
  char *query_key = create_query_key(POST_NS, post_id, TAGS_FIELD_NAME);
  DBList *post_tags = dbapi_lrange(query_key, 0, DB_UINT_MAX);
  free(query_key);

  return post_tags;
}

DBList *get_posts_by_tag(const char *tag_id)
{
  DBList *filtered_post_ids = create_dblist();
  DBList *post_ids = get_post_ids();
  DBListNode *post_id_node = post_ids->head;

  while (post_id_node)
  {
    const char *post_id = post_ids->head->data->value.string;
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
  return get_namespace_ids(TAG_NS_PREFIX, NAME_FIELD_SUFFIX);
}

char *create_tag(const char *name)
{
  char *oid = generate_oid();

  char *query_key = create_query_key(TAG_NS, oid, NAME_FIELD_NAME);
  dbapi_set(query_key, name);
  free(query_key);

  return oid;
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
  util_dbapi_set_list(query_key, tags);
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
  util_dbapi_set_list(query_key, tags);
  free(query_key);

  return true; // 成功更新 p_tags，返回 true
}

void flush_all(void)
{
  dbapi_flushall();
}
