#include "s_algorithm.h"
#include "social_network.h"
#include "database.h"
#include "db/list.h"
#include "db/hash.h"
#include <math.h>

#define selu_to_zero 0.01
#define selu_to_less 0.1

#define sigmoid_l 1
#define sigmoid_k 1
#define sigmoid_mid 0.5

// 小於relu_cut的權重直接變0
#define relu_cut 0.01

// 1.33/1 = 0.25
#define curve_begin 0.25

DBList *generate_personality(const DBHash *likes_dict)
{
  DBList *all_posts = ht_keys(likes_dict, NULL);
  DBList *all_id = get_post_ids();
  DBList *new_p_tag = create_dblist();

  DBListNode *post_node = all_posts->head;

  free_dblist(all_posts);
  free_dblist(all_id);
  return new_p_tag;
}

typedef double (*algorithm_exe)(double);

algorithm_exe algorithm[algorithm_count] = {
    s_selu,
    s_sigmoid,
    s_d_sigmoid,
    s_square};

void *SIGMOID(DBList *old_p_tag, const DBHash *likes_dict, const size_t iteration_i, const size_t iteration_n)
{
  DBList *new_p_tag = generate_personality(likes_dict);
  s_aggregate_cut_repair(old_p_tag, new_p_tag, iteration_i, iteration_n, sigmoid);
}
void *SELU(DBList *old_p_tag, const DBHash *likes_dict, const size_t iteration_i, const size_t iteration_n)
{
  DBList *new_p_tag = generate_personality(likes_dict);
  s_aggregate_cut_repair(old_p_tag, new_p_tag, iteration_i, iteration_n, selu);
}
void *D_SIGMOID(DBList *old_p_tag, const DBHash *likes_dict, const size_t iteration_i, const size_t iteration_n)
{
  DBList *new_p_tag = generate_personality(likes_dict);
  s_aggregate_cut_repair(old_p_tag, new_p_tag, iteration_i, iteration_n, d_sigmoid);
}
void *SQUARE(DBList *old_p_tag, const DBHash *likes_dict, const size_t iteration_i, const size_t iteration_n)
{
  DBList *new_p_tag = generate_personality(likes_dict);
  s_aggregate_cut_repair(old_p_tag, new_p_tag, iteration_i, iteration_n, square);
}

void s_aggregate(DBList *old_p_tag, DBList *new_p_tag, size_t time, size_t limit, type_of_algorithm s_type)
{
  DBListNode *old_node = old_p_tag->head;
  DBListNode *new_node = new_p_tag->head;

  TagWithWeight *old_string = create_tag_with_weight("1", 1);
  TagWithWeight *new_string = create_tag_with_weight("1", 1);

  while (old_node)
  {
    old_string = parse_tag_with_weight(old_node->data->value.string);
    new_string = parse_tag_with_weight(new_node->data->value.string);

    old_string->weight = (1 - linear(time / limit)) * old_string->weight;
    old_string->weight += linear(time / limit) * algorithm[s_type](new_string->weight);

    old_node->data->value.string = serialize_tag_with_weight(old_string);

    old_node = old_node->next;
    new_node = new_node->next;
  }

  free_tag_with_weight(old_string);
  free_tag_with_weight(new_string);
}

void s_cut(DBList *old_p_tag)
{
  DBListNode *node = old_p_tag->head;

  TagWithWeight *string = create_tag_with_weight('1', 1);

  while (node)
  {
    string = parse_tag_with_weight(node->data->value.string);
    string->weight = s_relu(string->weight);
    node->data->value.string = serialize_tag_with_weight(string);
    node = node->next;
  }

  free_tag_with_weight(string);
}

void s_repair(DBList *old_p_tag)
{
  DBListNode *node = old_p_tag->head;

  TagWithWeight *string = create_tag_with_weight('1', 1);

  double sum = 0;
  size_t count = 0;

  // 算總和，得出要除的比例
  while (node)
  {
    string = parse_tag_with_weight(node->data->value.string);
    sum += string->weight;
    node = node->next;
    count++;
  }

  node = old_p_tag->head;
  double propotion = sum / count;

  // 除完存回去
  while (node)
  {
    string = parse_tag_with_weight(node->data->value.string);
    string->weight /= propotion;
    node->data->value.string = serialize_tag_with_weight(string);
    node = node->next;
  }

  free_tag_with_weight(string);
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
  const double lambda = 1.05070098736;

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
  return input * sigmoid_k * f * (1 - f / sigmoid_l);
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

double curve(double input)
{
  return curve_begin * (input - 1) * (input - 1);
}
