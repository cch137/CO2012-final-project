#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "db/types.h"
#include "db/utils.h"
#include "db/hash.h"
#include "db/list.h"

#include "database.h"
#include "social_network.h"

#define TOTAL_USERS 5
#define TOTAL_POSTS 100000

static inline double drand()
{
  return (double)rand() / (double)RAND_MAX;
}

TagWithWeight *create_tag_with_weight(const char *tag_id, double weight)
{
  if (!tag_id)
    EXIT_ON_ERROR("Empty tag id");

  TagWithWeight *tag_with_weight = (TagWithWeight *)malloc(sizeof(TagWithWeight));
  if (tag_with_weight == NULL)
    EXIT_ON_MEMORY_ERROR();

  tag_with_weight->id = dbutil_strdup(tag_id);
  tag_with_weight->weight = weight;

  return tag_with_weight;
}

void free_tag_with_weight(TagWithWeight *tag_with_weight)
{
  if (!tag_with_weight)
    return;
  free((void *)tag_with_weight->id);
  free(tag_with_weight);
}

char *serialize_tag_with_weight(const TagWithWeight *tag_with_weight)
{
  if (!tag_with_weight || !tag_with_weight->id)
    EXIT_ON_ERROR("Empty or invalid TagWithWeight");

  // buffer size calculation:
  // = strlen(tag_id) + strlen(":") + strlen("0.000001") + 1
  // = strlen(tag_id) + 1 + 8 + 1
  // = strlen(tag_id) + 10
  size_t buffer_size = strlen(tag_with_weight->id) + 10;
  char *serialized = (char *)malloc(buffer_size);

  if (!serialized)
    EXIT_ON_MEMORY_ERROR();

  snprintf(serialized, buffer_size, "%s:%.6f", tag_with_weight->id, tag_with_weight->weight);

  return serialized;
}

TagWithWeight *parse_tag_with_weight(const char *tag_with_weight)
{
  if (tag_with_weight == NULL)
    EXIT_ON_ERROR("Empty TagWithWeight");

  // if ":" not found, `colon_pos` is NULL
  const char *colon_pos = strchr(tag_with_weight, ':');
  size_t tag_id_length = colon_pos ? colon_pos - tag_with_weight : strlen(tag_with_weight);
  char *tag_id = (char *)malloc(tag_id_length + 1);
  double weight = colon_pos ? atof(colon_pos + 1) : 0;

  if (!tag_id)
    EXIT_ON_MEMORY_ERROR();

  strncpy(tag_id, tag_with_weight, tag_id_length);
  tag_id[tag_id_length] = '\0';

  TagWithWeight *parsed_tag = create_tag_with_weight(tag_id, weight);

  free(tag_id);

  return parsed_tag;
}

void trim_ptags(DBList *ptags)
{
  double total_weight = 0;
  DBListNode *node = ptags->head;
  while (node)
  {
    TagWithWeight *tag_with_w = parse_tag_with_weight(node->data->value.string);
    total_weight += tag_with_w->weight;
    free_tag_with_weight(tag_with_w);
    node = node->next;
  }
  node = ptags->head;
  while (node)
  {
    TagWithWeight *tag_with_w = parse_tag_with_weight(node->data->value.string);
    tag_with_w->weight /= total_weight;
    node->data->value.string = serialize_tag_with_weight(tag_with_w);
    free_tag_with_weight(tag_with_w);
    node = node->next;
  }
}

// only use in init_social_network
static double _get_tag_prob_from_dict(const DBHash *tag_dict, const DBListNode *tag_id_node)
{
  const char *tag_id = tag_id_node->data->value.string;
  if (!tag_id)
    EXIT_ON_ERROR("Tag id is empty");
  const DBHashEntry *tag_probability_entry = hget(tag_dict, tag_id, NULL);
  if (!tag_probability_entry)
    EXIT_ON_ERROR("Tag probability not found");
  return tag_probability_entry->data->value.double_value;
}

void init_social_network(void)
{
  // clear database
  flush_all();

  // create tags (total: 15)
  DBHash *tag_dict = ht_create();
  hset(tag_dict, "tag-A", dbobj_create_double(0.70), NULL);
  hset(tag_dict, "tag-B", dbobj_create_double(0.70), NULL);
  hset(tag_dict, "tag-C", dbobj_create_double(0.50), NULL);
  hset(tag_dict, "tag-D", dbobj_create_double(0.50), NULL);
  hset(tag_dict, "tag-E", dbobj_create_double(0.30), NULL);
  hset(tag_dict, "tag-F", dbobj_create_double(0.20), NULL);
  hset(tag_dict, "tag-G", dbobj_create_double(0.20), NULL);
  hset(tag_dict, "tag-H", dbobj_create_double(0.10), NULL);
  hset(tag_dict, "tag-I", dbobj_create_double(0.10), NULL);
  hset(tag_dict, "tag-J", dbobj_create_double(0.10), NULL);
  hset(tag_dict, "tag-K", dbobj_create_double(0.10), NULL);
  hset(tag_dict, "tag-L", dbobj_create_double(0.10), NULL);
  hset(tag_dict, "tag-M", dbobj_create_double(0.10), NULL);
  hset(tag_dict, "tag-N", dbobj_create_double(0.10), NULL);
  hset(tag_dict, "tag-O", dbobj_create_double(0.10), NULL);
  DBList *tag_list = ht_keys(tag_dict, NULL);
  {
    DBListNode *tag_node = tag_list->head;
    while (tag_node)
    {
      create_tag(tag_node->data->value.string);
      tag_node = tag_node->next;
    }
  }

  // generate users
  for (int i = 0; i < TOTAL_USERS; ++i)
  {
    DBList *user_a_tags = create_dblist();
    DBListNode *tag_id_node = tag_list->head;
    while (tag_id_node)
    {
      // 計算使用者的到 tag 的概率
      const double again_tag_prob = _get_tag_prob_from_dict(tag_dict, tag_id_node);
      if (drand() < again_tag_prob)
      {
        const char tag_id = tag_id_node->data->value.string;
        const double tag_weight = drand(); // 計算使用者此 a_tag 的權重
        const TagWithWeight *tag_with_weight = create_tag_with_weight(tag_id, tag_weight);
        const char *a_tag_string = serialize_tag_with_weight(tag_with_weight);
        rpush(user_a_tags, create_dblistnode_with_string(a_tag_string));
        free_tag_with_weight(tag_with_weight);
      }
      tag_id_node = tag_id_node->next;
    }
    char user_name[10]; // format: user-xxxx
    sprintf(user_name, "user-%04X", i);
    create_user(user_name, user_a_tags);
    // TODO: 調查要不要 free user_a_tags
  }

  // generate posts
  for (int i = 0; i < TOTAL_POSTS; ++i)
  {
    DBList *post_tags = create_dblist();
    DBListNode *tag_id_node = tag_list->head;
    while (tag_id_node)
    {
      const char tag_id = tag_id_node->data->value.string;
      const double raw_tag_prob = _get_tag_prob_from_dict(tag_dict, tag_id_node);
      // 計算使用者獲得 tag 的概率，這裡會進行一些偏移操作。
      // 稍微降低帖子出現熱門 tag 的機率，稍微提升帖子出現冷門 tag 的機率。
      const double post_obtain_tag_prob = (raw_tag_prob + 0.5) / 2.0;
      if (drand() < post_obtain_tag_prob)
        rpush(post_tags, create_dblistnode_with_string(tag_id));
      tag_id_node = tag_id_node->next;
    }
    create_post(post_tags);
    // TODO: 調查要不要 free post_tags
  }

  ht_free(tag_dict);
  free_dblist(tag_list);

  return;
}

static double calculate_post_like_probability(const char post_id, const DBList *a_tags)
{
  double like_probability = 0;
  const DBList *post_tags = get_post_tags(post_id);

  DBListNode *post_tag_node = post_tags->head;
  while (post_tag_node)
  {
    const char *post_tag_id = post_tag_node->data->value.string;
    DBListNode *a_tag_node = a_tags->head;
    while (a_tag_node)
    {
      const TagWithWeight *tag_with_weight = parse_tag_with_weight(a_tag_node);
      // 帖文具備 user 的 a_tag，增加概率
      if (strcmp(tag_with_weight->id, post_tag_id) == 0)
      {
        like_probability += tag_with_weight->weight;
        if (like_probability >= 1)
          goto end;
      }
      free_tag_with_weight(tag_with_weight);
      a_tag_node = a_tag_node->next;
    }
    post_tag_node = post_tag_node->next;
  }
end:
  free_dblist(post_tags);
  return like_probability > 1 ? 1 : like_probability;
}

DBHash *get_user_feedback(const char *user_id, const DBList *post_ids)
{
  DBList *user_atags = get_user_atags(user_id);
  DBHash *likes_dict = ht_create();
  DBListNode *post_node = post_ids->head;

  while (post_node)
  {
    const char *post_id = post_node->data->value.string;

    if (drand() < calculate_post_like_probability(post_id, user_atags))
      hincrby(likes_dict, post_id, 1, NULL);

    post_node = post_node->next;
  }

  free_dblist(user_atags);

  return likes_dict;
}

DBHash *get_popular_feedback(const DBList *post_ids)
{
  const DBList *user_ids = get_user_ids();
  DBHash *users_likes_dict = ht_create();

  DBListNode *user_id_node = user_ids->head;
  while (user_id_node)
  {
    const char *user_id = user_id_node->data->value.string;
    const DBHash *user_likes_dict = get_user_feedback(user_id, post_ids);

    DBListNode *post_id_node = post_ids->head;
    while (post_id_node)
    {
      const char *post_id = post_id_node->data->value.string;
      const DBHashEntry *is_liked_entry = hget(user_likes_dict, post_id, NULL);
      if (is_liked_entry && strcmp(is_liked_entry->data->value.string, "1") == 0)
        hincrby(users_likes_dict, post_id, 1, NULL);
    }

    ht_free(user_likes_dict);
    user_id_node = user_id_node->next;
  }

  free_dblist(user_ids);

  return users_likes_dict;
}

void run_simulations(
    size_t posts_count,
    size_t iteration_count,
    RecommandPostsFunc recommanded_posts_func,
    AggregatePTagsFunc aggregate_func)
{
  const n = iteration_count;
  const DBList *user_ids = get_user_ids();

  {
    const DBList *post_ids = get_post_ids();
    const DBHash *likes_dict = get_popular_feedback(post_ids);
    const DBHash *ptag_dict = ht_create();

    // 把 likes_dict 轉換成 ptag_dict
    DBListNode *post_id_node = post_ids->head;
    while (post_id_node)
    {
      const char *post_id = post_id_node->data->value.string;
      const DBHashEntry *post_liked_entry = hget(likes_dict, post_id, NULL);
      dbobj_string_to_int(post_liked_entry->data);
      const int post_liked_count = post_liked_entry->data->value.int_value;
      const DBList *post_tags = get_post_tags(post_id);
      DBListNode *tag_node = post_tags->head;
      while (tag_node)
      {
        const char *tag_id = tag_node->data->value.string;
        hincrby(ptag_dict, tag_id, post_liked_count, NULL);
        tag_node = tag_node->next;
      }
      free_dblist(post_tags);
      post_id_node = post_id_node->next;
    }
    ht_free(likes_dict);
    free_dblist(post_ids);

    // 把 ptag_dict 轉換成 popular_ptags
    const DBList *popular_ptags = create_dblist();
    const DBList *ptag_ids = ht_keys(ptag_dict, NULL);
    DBListNode *ptag_id_node = ptag_ids->head;
    while (ptag_id_node)
    {
      const char *ptag_id = ptag_id_node->data->value.string;
      const DBHashEntry *ptag_entry = hget(ptag_dict, ptag_id, NULL);
      const int ptag_w = ptag_entry->data->value.int_value;
      const TagWithWeight *tag_with_weight = create_tag_with_weight(ptag_id, ptag_w);
      rpush(popular_ptags, serialize_tag_with_weight(tag_with_weight));
      free_tag_with_weight(tag_with_weight);
      ptag_id_node = ptag_id_node->next;
    }
    trim_ptags(popular_ptags);

    ht_free(ptag_dict);
    free_dblist(ptag_ids);

    // 將 popular_ptags 作為所有 user 的初始 ptags
    DBListNode *user_id_node = user_ids->head;
    while (user_id_node)
    {
      const char *user_id = user_id_node->data->value.string;
      set_user_ptags(user_id, popular_ptags);
      user_id_node = user_id_node->next;
    }
    free_dblist(popular_ptags);
  }

  // TODO: 每一輪 simulation 完成後，計算演算法評分
  for (size_t i = 0; i < n; ++i)
  {
    DBListNode *user_id_node = user_ids->head;
    while (user_id_node)
    {
      const char *user_id = user_id_node->data->value.string;
      const DBList *user_p_tags = get_user_ptags(user_id);
      const DBList *post_ids = recommanded_posts_func(user_id, posts_count, i, n);
      const DBHash *likes_dict = get_user_feedback(user_id, post_ids);
      aggregate_func(user_p_tags, likes_dict, i, n);
      set_user_ptags(user_id, user_p_tags);
      // TODO: 調查要不要 free user_p_tags
      ht_free(likes_dict);
      user_id_node = user_id_node->next;
    }
  }

  free_dblist(user_ids);
}
