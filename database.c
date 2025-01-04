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
#define POST_TAGS_KEY "tags"
#define USER_PTAGS_KEY "p_tags"

DBHash *main_ht = NULL;
DBHash *expr_ht = NULL;

static bool starts_with(const char *str, const char *prefix)
{
  size_t prefix_len = strlen(prefix);
  return strncmp(str, prefix, prefix_len) == 0;
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
  // 初始化必要變數
  char oid[13];
  generate_oid(oid); // 生成唯一 OID
  char *user_id = (char *)malloc(strlen(USER_NS_PREFIX) + strlen(oid) + 1);
  if (!user_id)
    EXIT_ON_MEMORY_ERROR();

  sprintf(user_id, "%s%s", USER_NS_PREFIX, oid); // 組合 user_id

  // 儲存 user:name
  if (name && strlen(name) > 0)
  {
    char user_name_key[strlen(user_id) + strlen(USER_NAME_KEY) + 2];
    sprintf(user_name_key, "%s:%s", user_id, USER_NAME_KEY);

    if (!dbapi_set(user_name_key, name))
    {
      fprintf(stderr, "Failed to store user name for %s\n", user_id);
      free(user_id);
      return NULL;
    }
  }

  // 儲存 user:a_tags
  if (a_tags && a_tags->length > 0)
  {
    char user_atags_key[strlen(user_id) + strlen(USER_ATAGS_KEY) + 2];
    sprintf(user_atags_key, "%s:%s", user_id, USER_ATAGS_KEY);

    DBListNode *curr = a_tags->head;
    while (curr)
    {
      if (dbobj_is_string(curr->data))
      {
        if (!dbapi_lpush(user_atags_key, curr->data->value.string))
        {
          fprintf(stderr, "Failed to store user tags for %s\n", user_id);
          dbapi_del(USER_NAME_KEY); // 清理已儲存的 name
          free(user_id);
          return NULL;
        }
      }
      curr = curr->next;
    }
  }

  return user_id; // 成功返回 user_id
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
  // 初始化必要變數
  char oid[13];
  generate_oid(oid); // 生成唯一 OID
  char *post_id = (char *)malloc(strlen(POST_NS_PREFIX) + strlen(oid) + 1);
  if (!post_id)
    EXIT_ON_MEMORY_ERROR();

  // 組合成完整的 post_id
  sprintf(post_id, "%s%s", POST_NS_PREFIX, oid);

  // 儲存 post:tags
  if (tags && tags->length > 0)
  {
    char post_tags_key[strlen(post_id) + strlen(POST_TAGS_KEY) + 2];
    sprintf(post_tags_key, "%s:%s", post_id, POST_TAGS_KEY);

    DBListNode *curr = tags->head;
    while (curr)
    {
      if (dbobj_is_string(curr->data))
      {
        if (!dbapi_lpush(post_tags_key, curr->data->value.string))
        {
          free(post_id);
          return NULL; // 若儲存失敗，回傳 NULL
        }
      }
      curr = curr->next;
    }
  }

  return post_id; // 回傳成功創建的 post_id
}

DBList *get_post_tags(const char *post_id)
{
  if (!post_id || !main_ht)
    return NULL;

  // 組合 post:tags 的鍵
  char post_tags_key[strlen(post_id) + strlen(POST_TAGS_KEY) + 2];
  sprintf(post_tags_key, "%s:%s", post_id, POST_TAGS_KEY);

  // 使用資料庫 API 獲取 tags 的 List
  DBList *tags_list = dbapi_lrange(post_tags_key, 0, DB_UINT_MAX);
  if (!tags_list)
    return NULL;

  // 確保返回的 tags 是一個新的複製品
  DBList *tags_copy = create_dblist();
  DBListNode *curr = tags_list->head;
  while (curr)
  {
    if (dbobj_is_string(curr->data))
    {
      rpush(tags_copy, create_dblistnode_with_string(dbutil_strdup(curr->data->value.string)));
    }
    curr = curr->next;
  }

  free_dblist(tags_list); // 釋放原始資料庫的返回結果
  return tags_copy;       // 返回複製後的標籤列表
}

DBList *get_posts_by_tag(const char *tag_id)
{
  if (!tag_id || !main_ht)
    return NULL;

  // 組合標籤對應的鍵，例如 "tag:technology:posts"
  char tag_posts_key[strlen(tag_id) + strlen(":posts") + 1];
  sprintf(tag_posts_key, "%s:posts", tag_id);

  // 使用資料庫 API 獲取與標籤相關的所有貼文 ID
  DBList *post_ids = dbapi_lrange(tag_posts_key, 0, DB_UINT_MAX);
  if (!post_ids)
    return NULL;

  // 確保返回的結果為新複製列表
  DBList *posts_copy = create_dblist();
  DBListNode *curr = post_ids->head;
  while (curr)
  {
    if (dbobj_is_string(curr->data))
    {
      rpush(posts_copy, create_dblistnode_with_string(dbutil_strdup(curr->data->value.string)));
    }
    curr = curr->next;
  }

  free_dblist(post_ids); // 釋放原始資料庫返回的列表
  return posts_copy;     // 返回複製的結果列表
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
  if (!name || strlen(name) == 0)
    return NULL; // 標籤名稱為空，返回 NULL

  // 生成唯一 OID
  char oid[13];
  generate_oid(oid);

  // 組合標籤的鍵，例如 "tag:abc123"
  char *tag_id = (char *)malloc(strlen(TAG_NS_PREFIX) + strlen(oid) + 1);
  if (!tag_id)
    EXIT_ON_MEMORY_ERROR();

  sprintf(tag_id, "%s%s", TAG_NS_PREFIX, oid);

  // 組合標籤名稱鍵，例如 "tag:abc123:name"
  char tag_name_key[strlen(tag_id) + strlen(":name") + 1];
  sprintf(tag_name_key, "%s:name", tag_id);

  // 使用 API 儲存標籤名稱
  if (!dbapi_set(tag_name_key, name))
  {
    free(tag_id); // 清理已分配的記憶體
    return NULL;  // 若儲存失敗，返回 NULL
  }

  // 初始化貼文列表的鍵，例如 "tag:abc123:posts"
  char tag_posts_key[strlen(tag_id) + strlen(":posts") + 1];
  sprintf(tag_posts_key, "%s:posts", tag_id);

  // 如果資料庫不需要顯式初始化列表，可省略此步
  if (!dbapi_lcreate(tag_posts_key))
  {
    // 若初始化失敗，清理標籤名稱
    dbapi_del(tag_name_key); // 刪除已儲存的名稱
    free(tag_id);
    return NULL;
  }

  return tag_id; // 成功創建標籤，返回其 ID
}

DBList *get_user_atags(const char *user_id)
{
  if (!user_id)
    return NULL;

  char user_atags_key[strlen(user_id) + strlen(USER_ATAGS_KEY) + 2];
  sprintf(user_atags_key, "%s:%s", user_id, USER_ATAGS_KEY);

  return dbapi_lrange(user_atags_key, 0, -1); // 從資料庫中獲取完整的 List
}

db_bool_t set_user_atags(const char *user_id, DBList *tags)
{
  if (!user_id || !tags)
    return false;

  char user_atags_key[strlen(user_id) + strlen(USER_ATAGS_KEY) + 2];
  sprintf(user_atags_key, "%s:%s", user_id, USER_ATAGS_KEY);

  // 先清空現有的資料
  if (!dbapi_del(user_atags_key))
    return false;

  // 新增新的 tags
  DBListNode *curr = tags->head;
  while (curr)
  {
    if (dbobj_is_string(curr->data))
    {
      if (!dbapi_lpush(user_atags_key, curr->data->value.string))
        return false; // 插入失敗時回傳 false
    }
    curr = curr->next;
  }

  return true;
}

DBList *get_user_ptags(const char *user_id)
{
  if (!user_id || !main_ht)
    return NULL;

  // 從主 Hash Table 獲取使用者數據
  DBHashEntry *entry = hget(main_ht, user_id, expr_ht);
  if (!entry || !dbobj_is_hash(entry->data))
    return NULL;

  DBHash *user_data = entry->data->value.hash;

  // 從使用者數據中獲取 PTags
  DBHashEntry *ptags_entry = hget(user_data, USER_PTAGS_KEY, NULL);
  if (!ptags_entry || !dbobj_is_list(ptags_entry->data))
    return NULL;

  // 複製標籤列表，確保調用者負責釋放內存
  DBList *ptags_copy = create_dblist();
  DBListNode *curr = ptags_entry->data->value.list->head;

  while (curr)
  {
    if (dbobj_is_string(curr->data))
    {
      rpush(ptags_copy, create_dblistnode_with_string(dbutil_strdup(curr->data->value.string)));
    }
    curr = curr->next;
  }

  return ptags_copy;
}

db_bool_t set_user_ptags(const char *user_id, DBList *tags)
{
  if (!user_id || !tags || !main_ht)
    return false;

  // 從主 Hash Table 獲取使用者數據
  DBHashEntry *entry = hget(main_ht, user_id, expr_ht);
  if (!entry || !dbobj_is_hash(entry->data))
    return false;

  DBHash *user_data = entry->data->value.hash;

  // 創建一個新的標籤列表
  DBList *tags_copy = create_dblist();
  DBListNode *curr = tags->head;

  while (curr)
  {
    if (dbobj_is_string(curr->data))
    {
      rpush(tags_copy, create_dblistnode_with_string(dbutil_strdup(curr->data->value.string)));
    }
    curr = curr->next;
  }

  // 將新標籤列表存入使用者數據
  DBHashEntry *ptags_entry = hget(user_data, USER_PTAGS_KEY, NULL);
  if (ptags_entry)
  {
    free_dbobj(ptags_entry->data); // 清理舊數據
    ptags_entry->data = dbobj_create_list(tags_copy);
  }
  else
  {
    hset(user_data, USER_PTAGS_KEY, dbobj_create_list(tags_copy), NULL);
  }

  return true;
}

// 清空整個資料庫
void flush_all(void)
{
  // 確保主 Hash Table 和過期 Hash Table 已初始化
  if (!main_ht)
    main_ht = ht_create();
  if (!expr_ht)
    expr_ht = ht_create();

  // 重置主 Hash Table 和過期 Hash Table
  ht_reset(main_ht);
  ht_reset(expr_ht);
}
