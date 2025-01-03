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

static bool starts_with(const char *str, const char *prefix)
{
  size_t prefix_len = strlen(prefix);
  return strncmp(str, prefix, prefix_len) == 0;
}

// Redis 連接對象
static redisContext *redis_conn = NULL;

// 初始化 Redis 連接
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
static void generate_oid(char *oid)
{
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  int timestamp = (int)time(NULL);
  sprintf(oid, "%06X", timestamp); // 前 6 位為時間戳
  for (int i = 0; i < 6; i++)
  {
    oid[6 + i] = charset[rand() % (sizeof(charset) - 1)];
  }
  oid[12] = '\0';
}

//----------------------------------
// User 模組
//----------------------------------

// 取得 使用者 IDs
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
}

//----------------------------------
// Post 模組
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
// Tag 模組
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

// 清空整個資料庫
void flush_all(void)
{
}
