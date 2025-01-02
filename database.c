// database.c
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <hiredis/hiredis.h>
#include <ctype.h>

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

// 取得符合 user:* pattern 的使用者 IDs，最多取 limit 筆。
char **get_user_ids(size_t limit)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "SCAN 0 MATCH user:* COUNT %zu", limit);
  if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements < 2)
  {
    fprintf(stderr, "Failed to fetch user IDs.\n");
    if (reply)
      freeReplyObject(reply);
    return NULL;
  }

  // SCAN 回傳結果結構：element[0] = 下一個 cursor、element[1] = 取得的 key 列表
  redisReply *keys_array = reply->element[1];
  size_t count = keys_array->elements;

  char **user_ids = malloc(sizeof(char *) * count);
  for (size_t i = 0; i < count; i++)
  {
    // 跳過 "user:" 前綴
    // 逐一把「user:xxxxx」轉成「xxxxx」
    user_ids[i] = strdup(keys_array->element[i]->str + 5);
  }

  freeReplyObject(reply);
  return user_ids;
}

size_t count_users(void) // 計算所有符合 user:* 的 key 數量。
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "KEYS user:*");
  size_t count = reply ? reply->elements : 0;
  if (reply)
    freeReplyObject(reply);
  return count;
}

char *create_user(const char *name) // 建立一個新使用者，並傳回該使用者的 OID。
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

  // 此處將一些欄位先設為預設值：actual_tags, predicted_tags, viewed_posts, liked_posts 等
  redisReply *reply = redisCommand(
      redis_conn,
      "HMSET %s name %s actual_tags '{}' predicted_tags '{}' viewed_posts '[]' liked_posts '[]'",
      key,
      name ? name : "");
  if (reply)
    freeReplyObject(reply);

  return strdup(oid);
}

bool delete_user(const char *user_id) // 刪除指定 user_id 的使用者。
{
  init_redis();
  char key[64];
  sprintf(key, "user:%s", user_id);

  redisReply *reply = redisCommand(redis_conn, "DEL %s", key);
  bool success = (reply && reply->integer > 0);
  if (reply)
    freeReplyObject(reply);

  return success;
}

bool rename_user(const char *user_id, const char *new_name)
{
  init_redis();
  if (!is_valid_key(new_name))
  {
    fprintf(stderr, "Invalid new user name.\n");
    return false;
  }

  char key[64];
  sprintf(key, "user:%s", user_id);
  redisReply *reply = redisCommand(redis_conn, "HSET %s name %s", key, new_name);
  bool success = reply && reply->integer > 0;
  if (reply)
    freeReplyObject(reply);

  return success;
}

char *get_user_by_name(const char *name)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "SCAN 0 MATCH user:*");
  if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements < 2)
  {
    if (reply)
      freeReplyObject(reply);
    return NULL;
  }

  // 取出實際的 keys list
  redisReply *keys_array = reply->element[1];
  if (keys_array->type != REDIS_REPLY_ARRAY)
  {
    freeReplyObject(reply);
    return NULL;
  }

  char *user_id = NULL;

  for (size_t i = 0; i < keys_array->elements; i++)
  {
    const char *redis_key = keys_array->element[i]->str; // e.g. "user:123"
    if (!redis_key)
      continue; // 保險檢查

    // 查詢這個 key 的 name 欄位
    redisReply *name_reply = redisCommand(redis_conn, "HGET %s name", redis_key);
    if (name_reply && name_reply->type == REDIS_REPLY_STRING)
    {
      if (strcmp(name_reply->str, name) == 0)
      {
        // 找到符合的 user
        user_id = strdup(redis_key + 5); // 跳過 "user:"
        freeReplyObject(name_reply);
        break;
      }
    }
    if (name_reply)
      freeReplyObject(name_reply);
  }

  freeReplyObject(reply);
  return user_id;
}

char **get_user_posts(const char *user_id, size_t *out_count)
{
  init_redis();
  char key[64];
  sprintf(key, "user:%s:viewed_posts", user_id);

  redisReply *reply = redisCommand(redis_conn, "LRANGE %s 0 -1", key);
  if (!reply || reply->type != REDIS_REPLY_ARRAY)
  {
    *out_count = 0;
    if (reply)
      freeReplyObject(reply);
    return NULL;
  }

  char **posts = malloc(sizeof(char *) * reply->elements);
  for (size_t i = 0; i < reply->elements; i++)
  {
    posts[i] = strdup(reply->element[i]->str);
  }
  *out_count = reply->elements;

  freeReplyObject(reply);
  return posts;
}

bool mark_post_as_viewed_by_user(const char *user_id, const char *post_id)
{
  init_redis();
  char key[64];
  sprintf(key, "user:%s:viewed_posts", user_id);

  redisReply *reply = redisCommand(redis_conn, "RPUSH %s %s", key, post_id);
  bool success = (reply && reply->integer > 0);

  // 同時增加 post 的檢視數
  char post_key[64];
  sprintf(post_key, "post:%s", post_id);
  redisReply *reply2 = redisCommand(redis_conn, "HINCRBY %s viewed 1", post_key);
  if (reply2)
    freeReplyObject(reply2);

  if (reply)
    freeReplyObject(reply);
  return success;
}

bool mark_post_as_liked_by_user(const char *user_id, const char *post_id)
{
  init_redis();
  char key[64];
  sprintf(key, "user:%s:liked_posts", user_id);

  redisReply *reply = redisCommand(redis_conn, "RPUSH %s %s", key, post_id);
  bool success = (reply && reply->integer > 0);

  // 同時增加 post 的按讚數
  char post_key[64];
  sprintf(post_key, "post:%s", post_id);
  redisReply *reply2 = redisCommand(redis_conn, "HINCRBY %s liked 1", post_key);
  if (reply2)
    freeReplyObject(reply2);

  if (reply)
    freeReplyObject(reply);
  return success;
}

//----------------------------------
// Post 模組
//----------------------------------

char **get_post_ids(size_t *out_count)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "SCAN 0 MATCH post:*");
  if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements < 2)
  {
    *out_count = 0;
    if (reply)
      freeReplyObject(reply);
    return NULL;
  }

  redisReply *keys_array = reply->element[1];
  char **post_ids = malloc(sizeof(char *) * keys_array->elements);

  for (size_t i = 0; i < keys_array->elements; i++)
  {
    // 跳過 "post:"
    post_ids[i] = strdup(keys_array->element[i]->str + 5);
  }

  *out_count = keys_array->elements;
  freeReplyObject(reply);
  return post_ids;
}

size_t count_posts(void)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "KEYS post:*");
  size_t count = reply ? reply->elements : 0;
  if (reply)
    freeReplyObject(reply);
  return count;
}

//----------------------------------
// Tag 模組
//----------------------------------

char **get_tag_ids(size_t *out_count)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "SCAN 0 MATCH tag:*");
  if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements < 2)
  {
    *out_count = 0;
    if (reply)
      freeReplyObject(reply);
    return NULL;
  }

  redisReply *keys_array = reply->element[1];
  char **tag_ids = malloc(sizeof(char *) * keys_array->elements);

  for (size_t i = 0; i < keys_array->elements; i++)
  {
    // 跳過 "tag:"
    tag_ids[i] = strdup(keys_array->element[i]->str + 4);
  }

  *out_count = keys_array->elements;
  freeReplyObject(reply);
  return tag_ids;
}

size_t count_tags(void)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "KEYS tag:*");
  size_t count = reply ? reply->elements : 0;
  if (reply)
    freeReplyObject(reply);
  return count;
}

bool edit_tag(const char *tag_id, const char *new_text)
{
  init_redis();
  char key[64];
  sprintf(key, "tag:%s", tag_id);

  redisReply *reply = redisCommand(redis_conn, "HSET %s text %s", key, new_text);
  bool success = (reply && reply->integer > 0);
  if (reply)
    freeReplyObject(reply);
  return success;
}

//----------------------------------
// 標籤權重管理
//----------------------------------

bool post_increase_tag_weight(const char *post_id, const char *tag_id, int increment)
{
  init_redis();
  char key[64];
  sprintf(key, "post:%s:tags", post_id);

  redisReply *reply = redisCommand(redis_conn, "HINCRBY %s %s %d", key, tag_id, increment);
  bool success = (reply && reply->type == REDIS_REPLY_INTEGER);
  if (reply)
    freeReplyObject(reply);
  return success;
}

bool post_decrease_tag_weight(const char *post_id, const char *tag_id, int decrement)
{
  return post_increase_tag_weight(post_id, tag_id, -decrement);
}

//----------------------------------
// 推薦系統與工具
//----------------------------------

char **get_posts_by_tag(const char *tag_id, size_t limit, size_t *out_count)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn,
                                   "ZREVRANGEBYSCORE posts_by_tag:%s +inf -inf LIMIT 0 %zu", tag_id, limit);

  if (!reply || reply->type != REDIS_REPLY_ARRAY)
  {
    *out_count = 0;
    if (reply)
      freeReplyObject(reply);
    return NULL;
  }

  char **posts = malloc(sizeof(char *) * reply->elements);
  for (size_t i = 0; i < reply->elements; i++)
  {
    posts[i] = strdup(reply->element[i]->str);
  }
  *out_count = reply->elements;
  freeReplyObject(reply);
  return posts;
}

// 支援動態索引
bool create_index_for_tags(void)
{
  init_redis();
  redisReply *reply = redisCommand(
      redis_conn,
      "EVAL \"for i, k in pairs(redis.call('KEYS', 'post:*')) do "
      "  for tag, score in pairs(redis.call('HGETALL', k .. ':tags')) do "
      "    redis.call('ZADD', 'posts_by_tag:' .. tag, tonumber(score), k) "
      "  end "
      "end\" 0");

  bool success = (reply && reply->type == REDIS_REPLY_STATUS);
  if (reply)
    freeReplyObject(reply);

  return success;
}
