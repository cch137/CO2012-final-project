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

size_t count_users(void) // 計算所有符合 user:* 的 key 數量。
{
}

char *create_user(const char *name) // 建立一個新使用者，並傳回該使用者的 OID。
{
}

bool delete_user(const char *user_id) // 刪除指定 user_id 的使用者。
{
}

bool rename_user(const char *user_id, const char *new_name)
{
}

char *get_user_by_name(const char *name)
{
}

char **get_user_posts(const char *user_id, size_t *out_count)
{
}

bool mark_post_as_viewed_by_user(const char *user_id, const char *post_id)
{
}

bool mark_post_as_liked_by_user(const char *user_id, const char *post_id)
{
}

//----------------------------------
// Post 模組
//----------------------------------

char **get_post_ids(size_t *out_count)
{
}

size_t count_posts(void)
{
}

//----------------------------------
// Tag 模組
//----------------------------------

char **get_tag_ids(size_t *out_count)
{
}

size_t count_tags(void)
{
}

bool edit_tag(const char *tag_id, const char *new_text)
{
}

//----------------------------------
// 標籤權重管理
//----------------------------------

bool post_increase_tag_weight(const char *post_id, const char *tag_id, int increment)
{
}

bool post_decrease_tag_weight(const char *post_id, const char *tag_id, int decrement)
{
}

//----------------------------------
// 推薦系統與工具
//----------------------------------

char **get_posts_by_tag(const char *tag_id, size_t limit, size_t *out_count)
{
}

// 支援動態索引
bool create_index_for_tags(void)
{
}
