#ifndef ALGORITHMS
#define ALGORITHMS

// https://zh.wikipedia.org/zh-tw/%E6%BF%80%E6%B4%BB%E5%87%BD%E6%95%B0
// https://docs.google.com/document/d/1IQn-drdI7MEJm8X1O7h2gmDl1pjV78Kul_Yg95b6mL0/edit?tab=t.0

#include <stddef.h>
#include "db/types.h"
#include "social_network.h"

typedef enum type_of_algorithm
{
  selu,
  sigmoid,
  d_sigmoid,
  square,
  direct,
  algorithm_count
} type_of_algorithm;

/*提供給main處理AggregatePTagsFunc */
void *SIGMOID(DBList *old_p_tags, DBHash *likes_dict, const size_t iteration_i, const size_t iteration_n);
void *SELU(DBList *old_p_tags, DBHash *likes_dict, const size_t iteration_i, const size_t iteration_n);
void *D_SIGMOID(DBList *old_p_tags, DBHash *likes_dict, const size_t iteration_i, const size_t iteration_n);
void *SQUARE(DBList *old_p_tags, DBHash *likes_dict, const size_t iteration_i, const size_t iteration_n);
void *DIRECT(DBList *old_p_tag, DBHash *likes_dict, const size_t iteration_i, const size_t iteration_n);

// 根據likes_dict產生p_tag
DBList *generate_personality(DBHash *likes_dict);

// 進行aggreate、cut、repair
void s_aggregate_cut_repair(DBList *old_p_tag, DBList *new_p_tag, size_t time, size_t limit, type_of_algorithm s_type);

/*前處理，將新舊p_tag混和*/
void s_aggregate(DBList *old_p_tag, DBList *new_p_tag, size_t time, size_t limit, type_of_algorithm s_type);

/*後處理，在新舊p_tag混和之後使用*/
typedef double alorithm_function(size_t input);

// selu與relu的混和，分段，將大的線性變大，小的指數變小
double s_selu(double input);

// 標準羅吉斯回歸函數，把x0變化
double s_sigmoid(double input);

// 回傳將input乘上在該點的斜率
double s_d_sigmoid(double input);

// 回傳input平方，希望讓大的放大，小的縮小
double s_square(double input);

// 通過二次函數讓更改的比例逐漸下降，y=0.25(x-1)^2
double s_curve(double input);

// 小於relu_cut的就為0
double s_relu(double input);

// 不做任何權重
double s_direct(double input);

// 讓p_tag總和變為1
void s_repair(DBList *old_p_tag);

#endif