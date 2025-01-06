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
#define TOTAL_POSTS 10000
#define BASIC_AGG_OLD_RATE (double)0.75
#define BASIC_AGG_NEW_RATE (double)0.25

static inline double drand()
{
  return (double)rand() / (double)RAND_MAX;
}

static bool starts_with(const char *str, const char *prefix)
{
  size_t prefix_len = strlen(prefix);
  return strncmp(str, prefix, prefix_len) == 0;
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
  double weight = colon_pos ? atof(colon_pos + 1) : 1;

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
static double _get_tag_prob_from_dict(DBHash *tag_dict, DBListNode *tag_id_node)
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
  // { [tag_name]: [tag_prob] }
  DBHash *tag_prob_dict = ht_create();
  // { [tag_name]: [tag_id] }
  DBHash *tag_id_dict = ht_create();
  hset(tag_prob_dict, "tag-A", dbobj_create_double(0.80), NULL);
  hset(tag_prob_dict, "tag-B", dbobj_create_double(0.80), NULL);
  hset(tag_prob_dict, "tag-C", dbobj_create_double(0.75), NULL);
  hset(tag_prob_dict, "tag-D", dbobj_create_double(0.70), NULL);
  hset(tag_prob_dict, "tag-E", dbobj_create_double(0.60), NULL);
  hset(tag_prob_dict, "tag-F", dbobj_create_double(0.50), NULL);
  hset(tag_prob_dict, "tag-G", dbobj_create_double(0.45), NULL);
  hset(tag_prob_dict, "tag-H", dbobj_create_double(0.40), NULL);
  hset(tag_prob_dict, "tag-I", dbobj_create_double(0.35), NULL);
  hset(tag_prob_dict, "tag-J", dbobj_create_double(0.25), NULL);
  hset(tag_prob_dict, "tag-K", dbobj_create_double(0.25), NULL);
  hset(tag_prob_dict, "tag-L", dbobj_create_double(0.20), NULL);
  hset(tag_prob_dict, "tag-M", dbobj_create_double(0.20), NULL);
  hset(tag_prob_dict, "tag-N", dbobj_create_double(0.20), NULL);
  hset(tag_prob_dict, "tag-O", dbobj_create_double(0.20), NULL);
  DBList *tag_name_list = ht_keys(tag_prob_dict, NULL);
  {
    DBListNode *tag_node = tag_name_list->head;
    while (tag_node)
    {
      const char *tag_name = tag_node->data->value.string;
      char *id = create_tag_with_id_returned(tag_name);
      hset(tag_id_dict, tag_name, dbobj_create_string(id), NULL);
      tag_node = tag_node->next;
    }
  }

  // generate users
  for (int i = 0; i < TOTAL_USERS; ++i)
  {
    DBList *user_atags = create_dblist();
    DBListNode *tag_name_node = tag_name_list->head;
    while (tag_name_node)
    {
      // 計算使用者的到 tag 的概率
      const double again_tag_prob = _get_tag_prob_from_dict(tag_prob_dict, tag_name_node);
      if (drand() < again_tag_prob)
      {
        const char *tag_name = tag_name_node->data->value.string;
        const char *tag_id = hget(tag_id_dict, tag_name, NULL)->data->value.string;
        const double tag_weight = drand(); // 計算使用者此 atag 的權重
        TagWithWeight *tag_with_w = create_tag_with_weight(tag_id, tag_weight);
        const char *atag_string = serialize_tag_with_weight(tag_with_w);
        rpush(user_atags, create_dblistnode_with_string(atag_string));
        free_tag_with_weight(tag_with_w);
      }
      tag_name_node = tag_name_node->next;
    }
    char user_name[10]; // format: user-xxxx
    sprintf(user_name, "user-%04X", i + 1);
    char *id = create_user_with_id_returned(user_name, user_atags);
    free(id);
    free_dblist(user_atags);
  }

  // generate posts
  for (int i = 0; i < TOTAL_POSTS; ++i)
  {
    DBList *post_tags = create_dblist();
    db_int_t selected_tag_index = (db_int_t)rand() % (db_int_t)tag_name_list->length;
    DBListNode *tag_name_node = tag_name_list->head;

    while (selected_tag_index && tag_name_node)
    {
      tag_name_node = tag_name_node->next;
      --selected_tag_index;
    }
    if (!tag_name_node)
      EXIT_ON_ERROR("Tag id node is NULL");
    const char *tag_name = tag_name_node->data->value.string;
    const char *tag_id = hget(tag_id_dict, tag_name, NULL)->data->value.string;
    rpush(post_tags, create_dblistnode_with_string(tag_id));

    // 以下是讓 post 獲取多個 tag 的方法，但目前不採用
    // while (tag_id_node)
    // {
    //   const char tag_id = tag_id_node->data->value.string;
    //   const double raw_tag_prob = _get_tag_prob_from_dict(tag_prob_dict, tag_id_node);
    //   // 計算使用者獲得 tag 的概率，這裡會進行一些偏移操作。
    //   // 稍微降低帖子出現熱門 tag 的機率，稍微提升帖子出現冷門 tag 的機率。
    //   const double post_obtain_tag_prob = (raw_tag_prob + 0.5) / 2.0;
    //   if (drand() < post_obtain_tag_prob)
    //     rpush(post_tags, create_dblistnode_with_string(tag_id));
    //   tag_id_node = tag_id_node->next;
    // }

    create_post(post_tags);
    free_dblist(post_tags);
  }

  ht_free(tag_prob_dict);
  ht_free(tag_id_dict);
  free_dblist(tag_name_list);

  return;
}

static void reset_users_ptags(DBList *popular_ptags)
{
  DBList *empty_ptags = create_dblist();
  DBList *user_ids = get_user_ids();
  DBListNode *user_id_node = user_ids->head;
  while (user_id_node)
  {
    const char *user_id = user_id_node->data->value.string;
    if (popular_ptags)
      set_user_ptags(user_id, popular_ptags);
    else
      set_user_ptags(user_id, empty_ptags);
    user_id_node = user_id_node->next;
  }
  free_dblist(empty_ptags);
  free_dblist(user_ids);
}

static DBList *likes_dict_to_ptags(DBHash *likes_dict)
{
  DBList *post_ids = ht_keys(likes_dict, NULL);
  DBHash *ptag_dict = ht_create();

  // 把 likes_dict 轉換成 ptag_dict
  DBListNode *post_id_node = post_ids->head;
  while (post_id_node)
  {
    const char *post_id = post_id_node->data->value.string;
    const DBHashEntry *post_liked_entry = hget(likes_dict, post_id, NULL);
    if (post_liked_entry)
      dbobj_string_to_int(post_liked_entry->data);
    const int post_liked_count = post_liked_entry ? post_liked_entry->data->value.int_value : 0;
    DBList *post_tags = get_post_tags(post_id);
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

  // 把 ptag_dict 轉換成 result_ptags
  DBList *result_ptags = create_dblist();
  DBList *ptag_ids = ht_keys(ptag_dict, NULL);
  DBListNode *ptag_id_node = ptag_ids->head;
  while (ptag_id_node)
  {
    const char *ptag_id = ptag_id_node->data->value.string;
    const DBHashEntry *ptag_entry = hget(ptag_dict, ptag_id, NULL);
    dbobj_string_to_int(ptag_entry->data);
    const double ptag_w = (double)ptag_entry->data->value.int_value / (double)post_ids->length;
    TagWithWeight *tag_with_w = create_tag_with_weight(ptag_id, ptag_w);
    char *serialized_ptags = serialize_tag_with_weight(tag_with_w);
    rpush(result_ptags, create_dblistnode_with_string(serialized_ptags));
    free(serialized_ptags);
    free_tag_with_weight(tag_with_w);
    ptag_id_node = ptag_id_node->next;
  }

  ht_free(ptag_dict);
  free_dblist(post_ids);
  free_dblist(ptag_ids);

  return result_ptags;
}

static double calculate_post_like_probability(const char *post_id, DBList *atags)
{
  double like_probability = 0;
  DBList *post_tags = get_post_tags(post_id);

  DBListNode *post_tag_node = post_tags->head;
  while (post_tag_node)
  {
    const char *post_tag_id = post_tag_node->data->value.string;
    DBListNode *atag_node = atags->head;
    while (atag_node)
    {
      const char *atag_id = atag_node->data->value.string;
      TagWithWeight *tag_with_weight = parse_tag_with_weight(atag_id);
      // 帖文具備 user 的 atag，增加概率
      if (strcmp(tag_with_weight->id, post_tag_id) == 0)
      {
        like_probability += tag_with_weight->weight;
        if (like_probability >= 1)
          goto end;
      }
      free_tag_with_weight(tag_with_weight);
      atag_node = atag_node->next;
    }
    post_tag_node = post_tag_node->next;
  }
end:
  free_dblist(post_tags);
  return like_probability > 1 ? 1 : like_probability;
}

DBHash *get_user_feedback(const char *user_id, DBList *post_ids)
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

DBHash *get_popular_feedback(DBList *post_ids)
{
  DBList *user_ids = get_user_ids();
  DBHash *users_likes_dict = ht_create();

  DBListNode *user_id_node = user_ids->head;
  while (user_id_node)
  {
    const char *user_id = user_id_node->data->value.string;
    DBHash *user_likes_dict = get_user_feedback(user_id, post_ids);

    DBListNode *post_id_node = post_ids->head;
    while (post_id_node)
    {
      const char *post_id = post_id_node->data->value.string;
      const DBHashEntry *is_liked_entry = hget(user_likes_dict, post_id, NULL);
      if (is_liked_entry && strcmp(is_liked_entry->data->value.string, "1") == 0)
        hincrby(users_likes_dict, post_id, 1, NULL);
      post_id_node = post_id_node->next;
    }

    ht_free(user_likes_dict);
    user_id_node = user_id_node->next;
  }

  free_dblist(user_ids);

  return users_likes_dict;
}

DBList *get_posts_by_ptags(DBList *ptags, size_t limit)
{
  DBList *posts = create_dblist();
  DBList *ptags_duplicated = create_dblist();
  DBListNode *ptag_node = ptags->head;
  while (ptag_node)
  {
    rpush(ptags_duplicated, create_dblistnode_with_string(ptag_node->data->value.string));
    ptag_node = ptag_node->next;
  }
  trim_ptags(ptags_duplicated);
  ptag_node = ptags_duplicated->head;
  while (ptag_node)
  {
    TagWithWeight *tag_with_w = parse_tag_with_weight(ptag_node->data->value.string);
    size_t post_count = (size_t)(limit * tag_with_w->weight);
    DBList *post_ids = get_posts_by_tag(tag_with_w->id, post_count);
    DBListNode *post_id_node = post_ids->head;
    while (post_id_node)
    {
      rpush(posts, create_dblistnode_with_string(post_id_node->data->value.string));
      post_id_node = post_id_node->next;
    }
    free_dblist(post_ids);
    free_tag_with_weight(tag_with_w);
    ptag_node = ptag_node->next;
  }
  free_dblist(ptags_duplicated);
  return posts;
}

DBList *basic_recommand_posts(
    DBList *ptags,
    size_t limit,
    size_t iteration_i,
    size_t iteration_n)
{
  return get_posts_by_ptags(ptags, limit);
}

void basic_aggregate_func(
    DBList *current_ptags,
    DBHash *likes_dict,
    size_t iteration_i,
    size_t iteration_n)
{
  DBList *offset_ptags = likes_dict_to_ptags(likes_dict);
  DBListNode *offset_ptag_node = offset_ptags->head;

  while (offset_ptag_node)
  {
    TagWithWeight *offset_ptag_with_w = parse_tag_with_weight(offset_ptag_node->data->value.string);
    DBListNode *curr_ptag_node = current_ptags->head;
    bool found = false;
    while (curr_ptag_node)
    {
      if (starts_with(curr_ptag_node->data->value.string, offset_ptag_with_w->id))
      {
        TagWithWeight *curr_ptag_with_w = parse_tag_with_weight(curr_ptag_node->data->value.string);
        curr_ptag_with_w->weight = (curr_ptag_with_w->weight * BASIC_AGG_OLD_RATE) + (offset_ptag_with_w->weight * BASIC_AGG_NEW_RATE);
        char *serialized_ptag = serialize_tag_with_weight(curr_ptag_with_w);
        free(curr_ptag_node->data->value.string);
        curr_ptag_node->data->value.string = serialized_ptag;
        free_tag_with_weight(curr_ptag_with_w);
        found = true;
        break;
      }
      curr_ptag_node = curr_ptag_node->next;
    }
    free_tag_with_weight(offset_ptag_with_w);
    offset_ptag_node = offset_ptag_node->next;
  }
  free_dblist(offset_ptags);
}

DBList *get_popular_tags()
{
  DBList *post_ids = get_post_ids();
  DBHash *likes_dict = get_popular_feedback(post_ids);
  free_dblist(post_ids);
  DBList *popular_ptags = likes_dict_to_ptags(likes_dict);

  return popular_ptags;
}

void run_simulations(
    size_t posts_recommanded_per_round,
    size_t iteration_count,
    RecommandPostsFunc recommanded_posts_func,
    AggregatePTagsFunc aggregate_func,
    DBList *popular_ptags)
{
  reset_users_ptags(popular_ptags);

  const size_t n = iteration_count;
  DBList *user_ids = get_user_ids();

  // TODO: 每一輪 simulation 完成後，計算演算法評分
  for (size_t i = 0; i < n; ++i)
  {
    printf("run simulation (%lu/%lu)\n", i + 1, n);
    DBListNode *user_id_node = user_ids->head;
    while (user_id_node)
    {
      const char *user_id = user_id_node->data->value.string;
      DBList *user_ptags = get_user_ptags(user_id);
      DBList *post_ids = recommanded_posts_func(user_ptags, posts_recommanded_per_round, i, n);
      DBHash *likes_dict = get_user_feedback(user_id, post_ids);
      aggregate_func(user_ptags, likes_dict, i, n);
      set_user_ptags(user_id, user_ptags);
      free_dblist(post_ids);
      ht_free(likes_dict);
      user_id_node = user_id_node->next;
    }
  }

  free_dblist(user_ids);
}
