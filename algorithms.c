#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "db/list.h"
#include "db/hash.h"

#include "database.h"
#include "social_network.h"
#include "algorithms.h"

#define initial_prpoption 0.2

#define selu_to_zero 0.01
#define selu_to_less 0.1

// 因為希望輸出的最大值接近1最小值接近0，所以調整k，原本應該是1
#define sigmoid_l 1
#define sigmoid_k 8
#define sigmoid_mid 0.5

// 小於relu_cut的權重直接變0
#define relu_cut 0.01

// 1.33/1 = 0.25
#define curve_begin 0.5

DBList *generate_personality(DBHash *likes_dict)
{
  DBList *all_viewed_posts = ht_keys(likes_dict, NULL);
  DBList *all_id = get_tag_ids();
  DBList *new_p_tag = create_dblist();

  DBListNode *post_node = all_viewed_posts->head;
  DBListNode *id_node = all_id->head;
  TagWithWeight *string = create_tag_w("1", 1);

  size_t post_count = 0;
  double liked_count = 0;

  while (id_node != NULL)
  {
    string->id = id_node->data->value.string;
    while (post_node != NULL)
    {
      DBList *a = get_post_tags(post_node->data->value.string);
      if (strcmp(string->id, a->head->data->value.string) == 0)
      {
        post_count++;
        liked_count += (strcmp(hget(likes_dict, post_node->data->value.string, NULL)->data->value.string, "0") == 0) ? 1 : 0;
      }
      post_node = post_node->next;
      free(a);
    }
    post_node = all_viewed_posts->head;
    if (post_count != 0)
    {
      string->weight = (double)liked_count / post_count;
    }
    else
    {
      string->weight = 0;
    }
    printf("%d %f\n", liked_count, post_count);
    printf("%s %f\n", string->id, string->weight);
    rpush(new_p_tag, create_dblistnode_with_string(serialize_tag_w(string)));
    id_node = id_node->next;
  }

  free_dblist(all_viewed_posts);
  free_dblist(all_id);

  s_print_dblist(new_p_tag);

  return new_p_tag;
}

DBList *add_random_to_tag(DBList *personal_p_tag, DBList *public_p_tag, double propotion)
{
  DBList *new_p_tag = create_dblist();
  TagWithWeight *new_string = create_tag_w("1", 1);

  if (propotion == -1)
  {
    propotion = initial_prpoption;
  }
  if (public_p_tag)
  {
    DBListNode *personal_node = personal_p_tag->head;
    DBListNode *public_node = public_p_tag->head;

    TagWithWeight *personal_string = create_tag_w("1", 1);
    TagWithWeight *public_string = create_tag_w("1", 1);
    while (public_node)
    {
      public_string = parse_tag_w(public_node->data->value.string);
      while (personal_node)
      {
        personal_string = parse_tag_w(personal_node->data->value.string);
        if (strcmp(public_string->id, personal_string->id) == 0)
        {
          new_string->weight = (1 - propotion) * personal_string->weight;
          new_string->weight += public_string->weight * propotion;

          rpush(new_p_tag, create_dblistnode_with_string(serialize_tag_w(new_string)));
          break;
        }
        personal_node = personal_node->next;
      }
      personal_node = personal_p_tag->head;
      public_node = public_node->next;
    }
    free_tag_w(personal_string);
    free_tag_w(public_string);
  }
  else
  {
    DBListNode *personal_node = personal_p_tag->head;
    TagWithWeight *personal_string = create_tag_w("1", 1);
    while (personal_node)
    {
      personal_string = parse_tag_w(personal_node->data->value.string);

      personal_string->weight = (1 - propotion) * personal_string->weight;
      personal_string->weight += propotion * ((double)rand() / (RAND_MAX + 1.0));

      personal_node = personal_node->next;
    }
    free_tag_w(personal_string);
  }
  free_tag_w(new_string);
  return new_p_tag;
}

typedef double (*algorithm_exe)(double);

algorithm_exe algorithm[algorithm_count] = {
    s_selu,
    s_sigmoid,
    s_d_sigmoid,
    s_square,
    s_direct};

void SIGMOID(DBList *old_p_tag, DBHash *likes_dict, size_t iteration_i, size_t iteration_n)
{
  DBList *new_p_tag = generate_personality(likes_dict);
  s_aggregate_cut_repair(old_p_tag, new_p_tag, iteration_i, iteration_n, sigmoid);
}

void SELU(DBList *old_p_tag, DBHash *likes_dict, size_t iteration_i, size_t iteration_n)
{
  DBList *new_p_tag = generate_personality(likes_dict);
  s_aggregate_cut_repair(old_p_tag, new_p_tag, iteration_i, iteration_n, selu);
}

void D_SIGMOID(DBList *old_p_tag, DBHash *likes_dict, size_t iteration_i, size_t iteration_n)
{
  DBList *new_p_tag = generate_personality(likes_dict);
  s_aggregate_cut_repair(old_p_tag, new_p_tag, iteration_i, iteration_n, d_sigmoid);
}
void SQUARE(DBList *old_p_tag, DBHash *likes_dict, size_t iteration_i, size_t iteration_n)
{
  DBList *new_p_tag = generate_personality(likes_dict);
  s_aggregate_cut_repair(old_p_tag, new_p_tag, iteration_i, iteration_n, square);
}
void DIRECT(DBList *old_p_tag, DBHash *likes_dict, size_t iteration_i, size_t iteration_n)
{
  DBList *new_p_tag = generate_personality(likes_dict);
  s_aggregate_cut_repair(old_p_tag, new_p_tag, iteration_i, iteration_n, direct);
}

void s_aggregate(DBList *old_p_tag, DBList *new_p_tag, size_t time, size_t limit, type_of_algorithm algo_type)
{
  DBListNode *old_node = old_p_tag->head;
  DBListNode *new_node = new_p_tag->head;

  TagWithWeight *old_string = create_tag_w("1", 1);
  TagWithWeight *new_string = create_tag_w("1", 1);
  while (new_node)
  {
    new_string = parse_tag_w(new_node->data->value.string);
    while (old_node)
    {
      old_string = parse_tag_w(old_node->data->value.string);
      if (strcmp(old_string->id, new_string->id) == 0)
      {
        old_string->weight = (1 - s_curve(time / limit)) * old_string->weight;
        old_string->weight += s_curve(time / limit) * algorithm[algo_type](new_string->weight);

        old_node->data->value.string = serialize_tag_w(old_string);

        break;
      }
      old_node = old_node->next;
    }
    old_node = old_p_tag->head;
    new_node = new_node->next;
  }
  free_tag_w(old_string);
  free_tag_w(new_string);
}

void s_cut(DBList *old_p_tag)
{
  DBListNode *node = old_p_tag->head;

  TagWithWeight *string = create_tag_w("1", 1);

  while (node)
  {
    string = parse_tag_w(node->data->value.string);
    string->weight = s_relu(string->weight);
    node->data->value.string = serialize_tag_w(string);
    node = node->next;
  }

  free_tag_w(string);
}

void s_repair(DBList *old_p_tag)
{
  DBListNode *node = old_p_tag->head;

  TagWithWeight *string = create_tag_w("1", 1);

  double sum = 0;
  size_t count = 0;

  // 算總和，得出要除的比例
  while (node)
  {
    string = parse_tag_w(node->data->value.string);
    sum += string->weight;
    node = node->next;
    count++;
  }

  node = old_p_tag->head;
  double propotion = sum / count;

  // 除完存回去
  while (node)
  {
    string = parse_tag_w(node->data->value.string);
    string->weight /= propotion;
    node->data->value.string = serialize_tag_w(string);
    node = node->next;
  }

  free_tag_w(string);
}

void s_aggregate_cut_repair(DBList *old_p_tag, DBList *new_p_tag, size_t time, size_t limit, type_of_algorithm s_type)
{
  s_aggregate(old_p_tag, new_p_tag, time, limit, s_type);
  s_cut(old_p_tag);
  // p_tag總和變1，不需要了
  // s_repair(old_p_tag);
  free_dblist(new_p_tag);
}

// 將大於selu_to_less的值放大alpha倍
// 小於selu_to_less的值呈指數下降
// 將小於selu_to_zero的值變為0
double s_selu(double input)
{
  // 常數定義
  const double alpha = 1.67326324235;
  const double lambda = 0.957; // 原本應該要是1.05多的但我們希望他最大輸出值是1所以更改

  if (input < selu_to_zero)
  {
    // 區間一：輸出為0
    return 0.0;
  }
  else if (input < selu_to_less)
  {
    // 區間二：指數增長，確保在 selu_to_zero 處連續
    return lambda * alpha * (exp(input - selu_to_less) - exp(selu_to_zero - selu_to_less));
  }
  else
  {
    // 區間三：線性增長，確保在 selu_to_less 處連續
    return lambda * input + (lambda * alpha * (1 - exp(selu_to_zero - selu_to_less)) - lambda * selu_to_less);
  }
}

// 標準羅吉斯回歸函數，把x0變化
double s_sigmoid(double input)
{
  return sigmoid_l / (1 + exp((-1) * sigmoid_k * (input - sigmoid_mid)));
}

// 回傳將input乘上在該點的斜率
double s_d_sigmoid(double input)
{
  double f = s_sigmoid(input);
  double derivative = sigmoid_k * sigmoid_l * f * (1 - f / sigmoid_l);
  // 平方歸一化，讓最大值為1並且最小值接近0
  double max_derivative = sigmoid_k * sigmoid_l / 4;
  double normalized = derivative / max_derivative;
  return normalized * normalized;
}

// 小於relu_cut的就為0
double s_relu(double input)
{
  return input > relu_cut ? input : 0;
}

// 回傳input平方，希望讓大的放大，小的縮小
double s_square(double input)
{
  return input * input;
}

double s_direct(double input)
{
  return input;
}

double s_curve(double input)
{
  return curve_begin * (input - 1) * (input - 1);
}

static void s_print_dblist(DBList *old_p_tag)
{
  DBListNode *node = old_p_tag->head;

  TagWithWeight *string = create_tag_w("1", 1);

  while (node)
  {
    string = parse_tag_w(node->data->value.string);
    printf("%s: %f\n", string->id, string->weight);
    node = node->next;
  }

  free_tag_w(string);
}