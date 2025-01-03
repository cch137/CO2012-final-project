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

#define USER_NS_PREFIX "user:"
#define POST_NS_PREFIX "post:"
#define TAG_NS_PREFIX "tag:"
#define USER_NAME_KEY "name"
#define USER_ATAGS_KEY "a_tags"
#define USER_NS_PREFIX "user:"

DBHash *main_ht = NULL;
DBHash *expr_ht = NULL;

static bool starts_with(const char *str, const char *prefix)
{
  size_t prefix_len = strlen(prefix);
  return strncmp(str, prefix, prefix_len) == 0;
}

// Redis 嚙踝蕭嚙踝蕭嚙賭��嚙踝蕭���嚙�
static redisContext *redis_conn = NULL;

// 嚙踝蕭嚙賣�迎蕭嚙踝蕭嚙� Redis 嚙踝蕭嚙踝蕭嚙踝蕭
static void init_redis()
{
  if (!redis_conn)
  {
    redis_conn = redisConnect("127.0.0.1", 6379);
    if (redis_conn == NULL || redis_conn->err)
    {
      fprintf(stderr, "Error connecting to Redis: %s\n", redis_conn ? redis_conn->errstr : "Connection error");
      exit(1);
    }
  }
}

// ���鈭�嚙賜��嚙質都嚙質��嚙踝蕭��潘撓嚙賭遛嚙質��嚙踝蕭嚙踝蕭嚙賜��嚙踝蕭嚙踝蕭
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

// ���鈭�嚙賜��嚙質都嚙質��嚙踝蕭嚙踝蕭嚙踝蕭嚙踝蕭嚙踝蕭��哨蕭嚙� OID
static void generate_oid(char *oid)
{
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  int timestamp = (int)time(NULL);
  sprintf(oid, "%06X", timestamp); // 嚙踝蕭嚙� 6 ��選蕭嚙踝蕭蝞賂蕭嚙踝蕭嚙踝蕭嚙踝蕭嚙�
  for (int i = 0; i < 6; i++)
  {
    oid[6 + i] = charset[rand() % (sizeof(charset) - 1)];
  }
  oid[12] = '\0';
}

//----------------------------------
// User ������嚙踝蕭
//----------------------------------

// 嚙踝蕭嚙賣�綽蕭 ��輯撒嚙賢�鳴蕭嚙� IDs
DBList *get_user_ids()
{
  DBList *list = dbapi_keys();
  DBList *user_ids = create_dblist();
  DBListNode *curr = list->head;

  while (curr)
  {
    if (!dbobj_is_string(curr->data))
      continue;
    if (starts_with(curr->data->value.string, USER_NS_PREFIX))
      rpush(user_ids, create_dblistnode_with_string(dbutil_strdup(curr->data->value.string)));
    curr = curr->next;
  }

  free_dblist(list);

  return user_ids;
};

char *create_user(const char *name, DBList *a_tags)
{
  init_redis();
  if (name && !is_valid_key(name))
  {
    fprintf(stderr, "Invalid user name.\n");
    return NULL;
  }

  char oid[13];
  generate_oid(oid);

  char key[64];
  sprintf(key, "user:%s", oid);

  // 嚙踝蕭���嚙踝蕭
  DBList atag[10];
  sprintf(atag, "a_tag:%d", oid);

  // Set the user's interest tags (a_tags) if provided
  if (a_tags)
  {
    DBList *tags_copy = create_dblist();
    DBListNode *curr = a_tags->head;
    while (curr)
    {
      if (dbobj_is_string(curr->data))
        rpush(tags_copy, create_dblistnode_with_string(dbutil_strdup(curr->data->value.string)));
      curr = curr->next;
    }
    hset(user_data, USER_ATAGS_KEY, dbobj_create_list(tags_copy), NULL);
  }

  // Save the user data into the database
  hset(main_ht, user_id, dbobj_create_hash(user_data), expr_ht);

  // Return the newly created user's ID
  return user_id;
}

//----------------------------------
// Post ������嚙踝蕭
//----------------------------------

DBList *get_post_ids()
{
  DBList *list = dbapi_keys();
  DBList *post_ids = create_dblist();
  DBListNode *curr = list->head;

  while (curr)
  {
    if (!dbobj_is_string(curr->data))
      continue;
    if (starts_with(curr->data->value.string, POST_NS_PREFIX))
      rpush(post_ids, create_dblistnode_with_string(dbutil_strdup(curr->data->value.string)));
    curr = curr->next;
  }

  free_dblist(list);

  return post_ids;
}

char *create_post(DBList *tags)
{
}

DBList *get_post_tags(const char *tag_id)
{
}

DBList *get_posts_by_tag(const char *tag_id)
{
}
//----------------------------------
// Tag ������嚙踝蕭
//----------------------------------

DBList *get_tag_ids()
{
  DBList *list = dbapi_keys();
  DBList *tag_ids = create_dblist();
  DBListNode *curr = list->head;

  while (curr)
  {
    if (!dbobj_is_string(curr->data))
      continue;
    if (starts_with(curr->data->value.string, TAG_NS_PREFIX))
      rpush(tag_ids, create_dblistnode_with_string(dbutil_strdup(curr->data->value.string)));
    curr = curr->next;
  }

  free_dblist(list);

  return tag_ids;
}

char *create_tag(const char *name)
{
}

DBList *get_user_atags(const char *user_id)
{
}

db_bool_t set_user_atags(const char *user_id, DBList *tags)
{
}

DBList *get_user_ptags(const char *user_id)
{
}

db_bool_t set_user_ptags(const char *user_id, DBList *tags)
{
}

// ���嚙質��蝞賂蕭皜賂蕭嚙賡��嚙踝蕭嚙踝蕭��剁蕭
void flush_all(void)
{
}
