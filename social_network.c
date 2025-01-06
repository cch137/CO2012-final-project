#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "db/types.h"
#include "db/interaction.h"
#include "db/utils.h"
#include "db/hash.h"
#include "db/list.h"

#include "database.h"
#include "social_network.h"

static inline double drand()
{
  return (double)rand() / (double)RAND_MAX;
}

static size_t count_posts_with_weight(size_t posts_count, double weight)
{
  size_t count = (size_t)round((double)posts_count * weight);
  if (count)
    return count;
  if (drand() < 0.5)
    return ceil((double)posts_count * weight);
  return 0;
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

void press_enter_to_continue()
{
  printf("Press Enter to continue...");
  fflush(stdout);
  while (getchar() != '\n')
    ;
}

void print_dblist(DBList *ptags)
{
  DBObj *obj = dbobj_create_list(ptags);
  print_dbobj(obj);
  obj->value.list = NULL;
  free_dbobj(obj);
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
        const double tag_weight = ceil((double)(rand() % 1000000)) / (double)1000000; // 計算使用者此 atag 的權重
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

static DBList *likes_dict_to_ptags(DBHash *likes_dict, size_t user_count)
{
  DBList *post_ids = ht_keys(likes_dict, NULL);
  // 當 tag 出現時，被按贊的次數
  DBHash *tag_likes_dict = ht_create();
  // tag 出現的總次數
  DBHash *tag_total_dict = ht_create();

  DBListNode *post_id_node = post_ids->head;
  while (post_id_node)
  {
    const char *post_id = post_id_node->data->value.string;
    const DBHashEntry *post_likes_entry = hget(likes_dict, post_id, NULL);
    if (post_likes_entry)
      dbobj_string_to_int(post_likes_entry->data);
    const int post_likes_count = post_likes_entry ? post_likes_entry->data->value.int_value : 0;
    if (post_likes_entry)
      dbobj_int_to_string(post_likes_entry->data);
    DBList *post_tags = get_post_tags(post_id);
    DBListNode *tag_node = post_tags->head;
    while (tag_node)
    {
      const char *tag_id = tag_node->data->value.string;
      hincrby(tag_likes_dict, tag_id, post_likes_count, NULL);
      hincrby(tag_total_dict, tag_id, user_count, NULL);
      tag_node = tag_node->next;
    }
    free_dblist(post_tags);
    post_id_node = post_id_node->next;
  }

  DBList *result_ptags = create_dblist();
  DBList *ptag_ids = ht_keys(tag_total_dict, NULL);
  DBListNode *ptag_id_node = ptag_ids->head;
  while (ptag_id_node)
  {
    const char *ptag_id = ptag_id_node->data->value.string;
    const DBHashEntry *tag_likes_entry = hget(tag_likes_dict, ptag_id, NULL);
    const DBHashEntry *tag_total_entry = hget(tag_total_dict, ptag_id, NULL);
    dbobj_string_to_int(tag_likes_entry->data);
    dbobj_string_to_int(tag_total_entry->data);
    const double ptag_likes_count = (double)tag_likes_entry->data->value.int_value;
    const double ptag_total_count = (double)tag_total_entry->data->value.int_value;
    dbobj_int_to_string(tag_likes_entry->data);
    dbobj_int_to_string(tag_total_entry->data);
    const double ptag_w = ptag_likes_count / ptag_total_count;
    TagWithWeight *tag_with_w = create_tag_with_weight(ptag_id, ptag_w);
    char *serialized_ptags = serialize_tag_with_weight(tag_with_w);
    rpush(result_ptags, create_dblistnode_with_string(serialized_ptags));
    free(serialized_ptags);
    free_tag_with_weight(tag_with_w);
    ptag_id_node = ptag_id_node->next;
  }

  ht_free(tag_likes_dict);
  ht_free(tag_total_dict);
  free_dblist(post_ids);
  free_dblist(ptag_ids);

  return result_ptags;
}

UserFeedback *create_user_feedback(
    DBHash *likes_dict_source,
    size_t users_count,
    size_t likes_count,
    size_t posts_count)
{
  UserFeedback *feedback = (UserFeedback *)malloc(sizeof(UserFeedback));
  if (!feedback)
    EXIT_ON_MEMORY_ERROR();

  feedback->likes_dict = likes_dict_source;
  feedback->ptags = likes_dict_to_ptags(likes_dict_source, users_count);
  feedback->users_count = users_count;
  feedback->likes_count = likes_count;
  feedback->posts_count = posts_count;
  feedback->likes_rate = (double)likes_count / (double)posts_count;

  return feedback;
}

void free_user_feedback(UserFeedback *feedback)
{
  if (!feedback)
    return;
  ht_free(feedback->likes_dict);
  free_dblist(feedback->ptags);
  free(feedback);
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

UserFeedback *get_user_feedback(const char *user_id, DBList *post_ids)
{
  DBList *user_atags = get_user_atags(user_id);
  DBHash *likes_dict = ht_create();
  DBListNode *post_node = post_ids->head;
  size_t likes_count = 0;
  size_t posts_count = 0;

  while (post_node)
  {
    const char *post_id = post_node->data->value.string;

    if (drand() < calculate_post_like_probability(post_id, user_atags))
    {
      hincrby(likes_dict, post_id, 1, NULL);
      ++likes_count;
    }
    else
    {
      hincrby(likes_dict, post_id, 0, NULL);
    }

    ++posts_count;
    post_node = post_node->next;
  }

  free_dblist(user_atags);

  return create_user_feedback(likes_dict, 1, likes_count, posts_count);
}

UserFeedback *get_popular_feedback(DBList *post_ids)
{
  bool pass_posts = true;

  if (!post_ids)
  {
    pass_posts = false;
    post_ids = get_post_ids();
  }

  DBList *user_ids = get_user_ids();
  DBHash *users_likes_dict = ht_create();
  size_t likes_count = 0;

  DBListNode *user_id_node = user_ids->head;
  while (user_id_node)
  {
    const char *user_id = user_id_node->data->value.string;
    UserFeedback *feedback = get_user_feedback(user_id, post_ids);

    DBListNode *post_id_node = post_ids->head;
    while (post_id_node)
    {
      const char *post_id = post_id_node->data->value.string;
      const DBHashEntry *is_liked_entry = hget(feedback->likes_dict, post_id, NULL);
      if (is_liked_entry)
      {
        dbobj_string_to_int(is_liked_entry->data);
        hincrby(users_likes_dict, post_id, is_liked_entry->data->value.int_value, NULL);
        dbobj_int_to_string(is_liked_entry->data);
      }
      else
        hincrby(users_likes_dict, post_id, 0, NULL);
      post_id_node = post_id_node->next;
    }

    likes_count += feedback->likes_count;
    free_user_feedback(feedback);
    user_id_node = user_id_node->next;
  }

  UserFeedback *feedback = create_user_feedback(users_likes_dict, user_ids->length, likes_count, post_ids->length);

  free_dblist(user_ids);

  if (!pass_posts)
    free_dblist(post_ids);

  return feedback;
}

DBList *get_posts_by_ptags(DBList *ptags, size_t count)
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
    size_t post_count = count_posts_with_weight(count, tag_with_w->weight);
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
    DBList *popular_ptags,
    size_t count,
    size_t iteration_i,
    size_t iteration_n)
{
  double iteration_rate = ((double)iteration_i + 0.5) / (double)(iteration_n);
  double base_weight = pow(iteration_rate, RCM_BASE_WEIGHT_EXP);
  if (base_weight > 1.0)
    base_weight = 1.0;
  double test_weight = 1.0 - base_weight;

  DBHash *recommanded_post_dict = ht_create();
  size_t base_part_count = count_posts_with_weight(count, base_weight);
  size_t test_part_count = count_posts_with_weight(count, test_weight);

  DBList *base_part = get_posts_by_ptags(ptags, base_part_count);
  DBList *test_part = get_posts_by_ptags(popular_ptags, test_part_count);

  DBListNode *part_node = base_part->head;
  while (part_node)
  {
    hset(recommanded_post_dict, part_node->data->value.string, dbobj_create_bool(true), NULL);
    part_node = part_node->next;
  }
  part_node = test_part->head;
  while (part_node)
  {
    hset(recommanded_post_dict, part_node->data->value.string, dbobj_create_bool(true), NULL);
    part_node = part_node->next;
  }

  DBList *recommanded_posts = ht_keys(recommanded_post_dict, NULL);

  ht_free(recommanded_post_dict);
  free_dblist(base_part);
  free_dblist(test_part);

  return recommanded_posts;
}

void basic_aggregate_func(
    DBList *current_ptags,
    UserFeedback *user_feedback,
    size_t iteration_i,
    size_t iteration_n)
{
  double iteration_rate = ((double)iteration_i + 0.5) / (double)(iteration_n);
  double old_weight_rate = 0.75 * pow(iteration_rate, 0.5) + 0.25;
  if (old_weight_rate > 1.0)
    old_weight_rate = 1.0;
  double new_weight_rate = 1.0 - old_weight_rate;

  DBListNode *offset_ptag_node = user_feedback->ptags->head;

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
        curr_ptag_with_w->weight = (curr_ptag_with_w->weight * old_weight_rate) + (offset_ptag_with_w->weight * new_weight_rate);
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
}

void run_simulations(
    size_t posts_recommanded_per_round,
    size_t iteration_count,
    RecommandPostsFunc recommanded_posts_func,
    AggregatePTagsFunc aggregate_func,
    DBList *popular_ptags)
{
  DBList *used_popular_ptags = create_dblist();

  if (popular_ptags)
  {
    DBListNode *tag_id_node = popular_ptags->head;
    while (tag_id_node)
    {
      const char *serialized_tag = tag_id_node->data->value.string;
      rpush(used_popular_ptags, create_dblistnode_with_string(serialized_tag));
      tag_id_node = tag_id_node->next;
    }
  }
  else
  {
    DBList *tag_ids = get_tag_ids();
    DBListNode *tag_id_node = tag_ids->head;
    while (tag_id_node)
    {
      TagWithWeight *tag_with_w = create_tag_with_weight(tag_id_node->data->value.string, 0.5);
      char *serialized_tag = serialize_tag_with_weight(tag_with_w);
      rpush(used_popular_ptags, create_dblistnode_with_string(serialized_tag));
      free(serialized_tag);
      free_tag_with_weight(tag_with_w);
      tag_id_node = tag_id_node->next;
    }
    free_dblist(tag_ids);
  }

  char *popular_id = get_user_id_by_name(POPULAR_USER_NAME);
  delete_user(popular_id);
  free(popular_id);
  reset_users_ptags(used_popular_ptags);

  const size_t n = iteration_count;
  DBList *user_ids = get_user_ids();
  const size_t users_count = user_ids->length;

  printf("start simulations\n");
  for (size_t i = 0; i < n; ++i)
  {
    double total_likes_rate = 0.0;
    DBListNode *user_id_node = user_ids->head;
    while (user_id_node)
    {
      const char *user_id = user_id_node->data->value.string;
      DBList *user_ptags = get_user_ptags(user_id);
      DBList *post_ids = recommanded_posts_func(user_ptags, used_popular_ptags, posts_recommanded_per_round, i, n);
      UserFeedback *feedback = get_user_feedback(user_id, post_ids);
      aggregate_func(user_ptags, feedback, i, n);
      total_likes_rate += feedback->likes_rate;
      set_user_ptags(user_id, user_ptags);
      free_dblist(user_ptags);
      free_dblist(post_ids);
      free_user_feedback(feedback);
      user_id_node = user_id_node->next;
    }
    printf("run simulation (%lu/%lu) likes=%d%%\n", i + 1, n, (int)((double)100 * (total_likes_rate / (double)users_count)));
  }

  // clean ptags
  DBListNode *user_id_node = user_ids->head;
  while (user_id_node)
  {
    const char *user_id = user_id_node->data->value.string;
    DBList *user_ptags = get_user_ptags(user_id);
    DBList *filtered_user_ptags = create_dblist();
    DBListNode *ptag_node = lpop(user_ptags);
    while (ptag_node)
    {
      TagWithWeight *tag_with_w = parse_tag_with_weight(ptag_node->data->value.string);
      if (tag_with_w->weight < CLEAN_PTAG_THRESHOLD)
        free_dblistnode(ptag_node);
      else
        rpush(filtered_user_ptags, ptag_node);
      free_tag_with_weight(tag_with_w);
      ptag_node = lpop(user_ptags);
    }
    set_user_ptags(user_id, filtered_user_ptags);
    free_dblist(user_ptags);
    free_dblist(filtered_user_ptags);
    user_id_node = user_id_node->next;
  }

  popular_id = create_user_with_id_returned(POPULAR_USER_NAME, used_popular_ptags);

  save_db();

  free(popular_id);
  free_dblist(user_ids);
  free_dblist(used_popular_ptags);
}
