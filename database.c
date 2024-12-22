#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

char **get_user_ids(size_t limit)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "SCAN 0 MATCH user:* COUNT %zu", limit);
  if (!reply || reply->type != REDIS_REPLY_ARRAY)
  {
    fprintf(stderr, "Failed to fetch user IDs.\n");
    freeReplyObject(reply);
    return NULL;
  }

  size_t count = reply->element[1]->elements;
  char **user_ids = malloc(sizeof(char *) * count);
  for (size_t i = 0; i < count; i++)
  {
    user_ids[i] = strdup(reply->element[1]->element[i]->str + 5); // 跳過 "user:"
  }
  freeReplyObject(reply);
  return user_ids;
}

size_t count_users(void)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "KEYS user:*");
  size_t count = reply ? reply->elements : 0;
  freeReplyObject(reply);
  return count;
}

char *create_user(const char *name)
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

  redisReply *reply = redisCommand(redis_conn, "HMSET %s name %s actual_tags '{}' predicted_tags '{}' viewed_posts '[]' liked_posts '[]'", key, name ? name : "");
  freeReplyObject(reply);

  return strdup(oid);
}

bool delete_user(const char *user_id)
{
  init_redis();
  char key[64];
  sprintf(key, "user:%s", user_id);
  redisReply *reply = redisCommand(redis_conn, "DEL %s", key);
  bool success = reply && reply->integer > 0;
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
  freeReplyObject(reply);
  return success;
}

char *get_user_by_name(const char *name)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "SCAN 0 MATCH user:*");
  if (!reply || reply->type != REDIS_REPLY_ARRAY)
  {
    freeReplyObject(reply);
    return NULL;
  }

  char *user_id = NULL;
  for (size_t i = 0; i < reply->element[1]->elements; i++)
  {
    char key[64];
    sprintf(key, "%s name", reply->element[1]->element[i]->str);
    redisReply *name_reply = redisCommand(redis_conn, "HGET %s name", key);
    if (name_reply && strcmp(name_reply->str, name) == 0)
    {
      user_id = strdup(reply->element[1]->element[i]->str + 5); // 跳過 "user:"
      freeReplyObject(name_reply);
      break;
    }
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
  bool success = reply && reply->integer > 0;

  // 同時增加 post 的檢視數
  char post_key[64];
  sprintf(post_key, "post:%s", post_id);
  redisCommand(redis_conn, "HINCRBY %s viewed 1", post_key);

  freeReplyObject(reply);
  return success;
}

bool mark_post_as_liked_by_user(const char *user_id, const char *post_id)
{
  init_redis();
  char key[64];
  sprintf(key, "user:%s:liked_posts", user_id);

  redisReply *reply = redisCommand(redis_conn, "RPUSH %s %s", key, post_id);
  bool success = reply && reply->integer > 0;

  // 同時增加 post 的按讚數
  char post_key[64];
  sprintf(post_key, "post:%s", post_id);
  redisCommand(redis_conn, "HINCRBY %s liked 1", post_key);

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
  if (!reply || reply->type != REDIS_REPLY_ARRAY)
  {
    *out_count = 0;
    freeReplyObject(reply);
    return NULL;
  }

  char **post_ids = malloc(sizeof(char *) * reply->element[1]->elements);
  for (size_t i = 0; i < reply->element[1]->elements; i++)
  {
    post_ids[i] = strdup(reply->element[1]->element[i]->str + 5); // Skip "post:"
  }
  *out_count = reply->element[1]->elements;
  freeReplyObject(reply);
  return post_ids;
}

size_t count_posts(void)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "KEYS post:*");
  size_t count = reply ? reply->elements : 0;
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
  if (!reply || reply->type != REDIS_REPLY_ARRAY)
  {
    *out_count = 0;
    freeReplyObject(reply);
    return NULL;
  }

  char **tag_ids = malloc(sizeof(char *) * reply->element[1]->elements);
  for (size_t i = 0; i < reply->element[1]->elements; i++)
  {
    tag_ids[i] = strdup(reply->element[1]->element[i]->str + 4); // Skip "tag:"
  }
  *out_count = reply->element[1]->elements;
  freeReplyObject(reply);
  return tag_ids;
}

size_t count_tags(void)
{
  init_redis();
  redisReply *reply = redisCommand(redis_conn, "KEYS tag:*");
  size_t count = reply ? reply->elements : 0;
  freeReplyObject(reply);
  return count;
}

bool edit_tag(const char *tag_id, const char *new_text)
{
  init_redis();
  char key[64];
  sprintf(key, "tag:%s", tag_id);

  redisReply *reply = redisCommand(redis_conn, "HSET %s text %s", key, new_text);
  bool success = reply && reply->integer > 0;
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
  bool success = reply && reply->integer > 0;
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
  redisReply *reply = redisCommand(redis_conn, "ZREVRANGEBYSCORE posts_by_tag:%s +inf -inf LIMIT 0 %zu", tag_id, limit);
  if (!reply || reply->type != REDIS_REPLY_ARRAY)
  {
    *out_count = 0;
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
  redisReply *reply = redisCommand(redis_conn, "EVAL \"for i, k in pairs(redis.call('KEYS', 'post:*')) do for tag, score in pairs(redis.call('HGETALL', k .. ':tags')) do redis.call('ZADD', 'posts_by_tag:' .. tag, tonumber(score), k) end end\" 0");
  bool success = reply && reply->type == REDIS_REPLY_STATUS;
  freeReplyObject(reply);
  return success;
}
