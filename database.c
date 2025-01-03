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
  // 確保資料庫初始化
  if (!main_ht)
    main_ht = ht_create();

  if (!expr_ht)
    expr_ht = ht_create();

  // 生成 OID
  char oid[13];
  generate_oid(oid);

  // 分配 user_id
  char *user_id = (char *)malloc(strlen(USER_NS_PREFIX) + strlen(oid) + 1);
  if (!user_id)
    EXIT_ON_MEMORY_ERROR();

  snprintf(user_id, strlen(USER_NS_PREFIX) + strlen(oid) + 1, "%s%s", USER_NS_PREFIX, oid);

  // 建立使用者資料
  DBHash *user_data = ht_create();
  if (!user_data)
  {
    free(user_id);
    EXIT_ON_MEMORY_ERROR();
  }

  // 處理 name
  if (name && strlen(name) > 0)
    hset(user_data, USER_NAME_KEY, dbobj_create_string_with_dup(name), NULL);

  // 處理 a_tags
  if (a_tags && a_tags->length > 0)
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

  // 儲存使用者資料到 main_ht
  hset(main_ht, user_id, dbobj_create_hash(user_data), expr_ht);

  return user_id; // 回傳使用者 ID
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
  // 確保主 Hash Table 和過期表已初始化
  if (!main_ht)
  {
    main_ht = ht_create();
  }

  if (!expr_ht)
  {
    expr_ht = ht_create();
  }

  // 生成貼文 OID
  char oid[13];
  generate_oid(oid);
  char *post_id = (char *)malloc(strlen(POST_NS_PREFIX) + strlen(oid) + 1);
  if (!post_id)
    EXIT_ON_MEMORY_ERROR();
  sprintf(post_id, "%s%s", POST_NS_PREFIX, oid);

  // 創建貼文數據結構
  DBHash *post_data = ht_create();

  if (tags)
  {
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
    hset(post_data, POST_TAGS_KEY, dbobj_create_list(tags_copy), NULL);
  }

  // 將貼文儲存到主 Hash Table
  hset(main_ht, post_id, dbobj_create_hash(post_data), expr_ht);

  return post_id;
}

DBList *get_post_tags(const char *post_id)
{
  if (!post_id)
    return NULL;

  // 確保主 Hash Table 已初始化
  if (!main_ht)
    return NULL;

  // 從主 Hash Table 獲取貼文數據
  DBHashEntry *entry = hget(main_ht, post_id, expr_ht);
  if (!entry || !dbobj_is_hash(entry->data))
    return NULL;

  DBHash *post_data = entry->data->value.hash;

  // 從貼文數據中獲取標籤列表
  DBHashEntry *tags_entry = hget(post_data, POST_TAGS_KEY, NULL);
  if (!tags_entry || !dbobj_is_list(tags_entry->data))
    return NULL;

  // 複製標籤列表，確保調用者負責釋放內存
  DBList *tags_copy = create_dblist();
  DBListNode *curr = tags_entry->data->value.list->head;
  while (curr)
  {
    if (dbobj_is_string(curr->data))
    {
      rpush(tags_copy, create_dblistnode_with_string(dbutil_strdup(curr->data->value.string)));
    }
    curr = curr->next;
  }

  return tags_copy;
}

DBList *get_posts_by_tag(const char *tag_id)
{
  if (!tag_id || !main_ht)
    return NULL;

  DBList *post_ids = dbapi_keys(); // 獲取所有的鍵值
  DBList *posts_with_tag = create_dblist();
  DBListNode *curr = post_ids->head;

  while (curr)
  {
    if (!dbobj_is_string(curr->data))
    {
      curr = curr->next;
      continue;
    }

    const char *post_id = curr->data->value.string;
    DBHashEntry *entry = hget(main_ht, post_id, expr_ht);

    if (!entry || !dbobj_is_hash(entry->data))
    {
      curr = curr->next;
      continue;
    }

    DBHash *post_data = entry->data->value.hash;
    DBHashEntry *tags_entry = hget(post_data, POST_TAGS_KEY, NULL);

    if (!tags_entry || !dbobj_is_list(tags_entry->data))
    {
      curr = curr->next;
      continue;
    }

    DBList *tags = tags_entry->data->value.list;
    DBListNode *tag_node = tags->head;
    while (tag_node)
    {
      if (dbobj_is_string(tag_node->data) &&
          strcmp(tag_node->data->value.string, tag_id) == 0)
      {
        rpush(posts_with_tag, create_dblistnode_with_string(dbutil_strdup(post_id)));
        break;
      }
      tag_node = tag_node->next;
    }

    curr = curr->next;
  }

  free_dblist(post_ids);
  return posts_with_tag;
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
  // 確保主 Hash Table 和過期表已初始化
  if (!main_ht)
  {
    main_ht = ht_create();
  }

  if (!expr_ht)
  {
    expr_ht = ht_create();
  }

  // 生成標籤 OID
  char oid[13];
  generate_oid(oid);
  char *tag_id = (char *)malloc(strlen(TAG_NS_PREFIX) + strlen(oid) + 1);
  if (!tag_id)
    EXIT_ON_MEMORY_ERROR();
  sprintf(tag_id, "%s%s", TAG_NS_PREFIX, oid);

  // 創建標籤數據結構
  DBHash *tag_data = ht_create();

  if (name && strlen(name) > 0)
  {
    hset(tag_data, "name", dbobj_create_string_with_dup(name), NULL);
  }

  // 將標籤存入主 Hash Table
  hset(main_ht, tag_id, dbobj_create_hash(tag_data), expr_ht);

  return tag_id;
}

DBList *get_user_atags(const char *user_id)
{
  if (!user_id || !main_ht)
    return NULL;

  // 從主 Hash Table 獲取使用者數據
  DBHashEntry *entry = hget(main_ht, user_id, expr_ht);
  if (!entry || !dbobj_is_hash(entry->data))
    return NULL;

  DBHash *user_data = entry->data->value.hash;

  // 從使用者數據中獲取興趣標籤列表
  DBHashEntry *atags_entry = hget(user_data, USER_ATAGS_KEY, NULL);
  if (!atags_entry || !dbobj_is_list(atags_entry->data))
    return NULL;

  // 複製興趣標籤列表，確保調用者負責釋放內存
  DBList *atags_copy = create_dblist();
  DBListNode *curr = atags_entry->data->value.list->head;

  while (curr)
  {
    if (dbobj_is_string(curr->data))
    {
      rpush(atags_copy, create_dblistnode_with_string(dbutil_strdup(curr->data->value.string)));
    }
    curr = curr->next;
  }

  return atags_copy;
}

db_bool_t set_user_atags(const char *user_id, DBList *tags)
{
  if (!user_id || !main_ht || !tags)
    return false;

  // 從主 Hash Table 獲取使用者數據
  DBHashEntry *entry = hget(main_ht, user_id, expr_ht);
  if (!entry || !dbobj_is_hash(entry->data))
    return false;

  DBHash *user_data = entry->data->value.hash;

  // 複製標籤列表，確保內存安全
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

  // 更新使用者數據中的興趣標籤
  DBHashEntry *atags_entry = hget(user_data, USER_ATAGS_KEY, NULL);
  if (atags_entry)
  {
    // 如果已有標籤，釋放舊的標籤數據
    free_dblist(atags_entry->data->value.list);
    atags_entry->data->value.list = tags_copy;
  }
  else
  {
    // 如果沒有標籤，新增標籤數據
    hset(user_data, USER_ATAGS_KEY, dbobj_create_list(tags_copy), NULL);
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
