#ifndef SOCIAL_NETWORK_H
#define SOCIAL_NETWORK_H

#define TOTAL_USERS 5
#define TOTAL_POSTS 1000

#define RCM_BASE_WEIGHT_EXP ((double)2.0)

#define CLEAN_PTAG_THRESHOLD ((double)0.005)

#include <stddef.h>
#include <stdbool.h>

#include "db/types.h"

typedef struct UserFeedback
{
  DBHash *likes_dict;
  DBList *ptags;
  size_t users_count;
  size_t likes_count;
  size_t posts_count;
  double likes_rate;
} UserFeedback;

UserFeedback *create_user_feedback(
    DBHash *likes_dict_source,
    size_t users_count,
    size_t likes_count,
    size_t posts_count);

void free_user_feedback(UserFeedback *feedback);

typedef struct
{
  const char *id;
  double weight;
} TagWithWeight;

// 就只是create
TagWithWeight *create_tag_with_weight(const char *tag_id, double weight);
// 用完tag_with_weight記得要free喔
void free_tag_with_weight(TagWithWeight *tag_with_weight);
// serialize 是把 struct 轉回 string
char *serialize_tag_with_weight(const TagWithWeight *tag_with_weight);
// parse 是把 string 轉成 struct
TagWithWeight *parse_tag_with_weight(const char *tag_with_weight);

void trim_ptags(DBList *ptags);

void press_enter_to_continue();

void print_dblist(DBList *ptags);

// 提供推薦貼文列表
// 根據使用者的預測標籤 (ptags)，回傳指定數量 (count) 的貼文 ID 列表。
// ptags 需由呼叫者負責釋放，count 為回傳的實際數量。
// iteration_i 表示當前迭代次數，iteration_n 表示總迭代次數。
typedef DBList *(*RecommandPostsFunc)(
    DBList *ptags,
    DBList *popular_ptags,
    size_t count,
    size_t iteration_i,
    size_t iteration_n);

// 計算新的 ptags 列表
// 根據貼文的獲贊記錄 (likes_dict)，更新 current_ptags 並回傳修改後的結果。
// likes_dict 的 key 為貼文 ID，value 為獲贊數 (整數字符串)。
// iteration_i 表示當前迭代次數，iteration_n 表示總迭代次數。
typedef void (*AggregatePTagsFunc)(
    DBList *current_ptags,
    UserFeedback *user_feedback,
    size_t iteration_i,
    size_t iteration_n);

// 初始化社群網路系統
// 清空資料庫並產生模擬的假資料。
void init_social_network(void);

// 獲取單一使用者的按贊行為
// 模擬使用者根據 atags 的機率對指定貼文 (post_ids) 進行按贊。
// 回傳的 Hash 中，key 為貼文 ID，value 為 "1" (贊) 或 "0" (未贊)。
UserFeedback *get_user_feedback(const char *user_id, DBList *post_ids);

// 獲取多名使用者的按贊統計
// 模擬所有使用者根據 atags 的機率對指定貼文 (post_ids) 進行按贊。
// 回傳的 Hash 中，key 為貼文 ID，value 為總獲贊數。
UserFeedback *get_popular_feedback(DBList *post_ids);

DBList *get_posts_by_ptags(DBList *ptags, size_t count);

DBList *basic_recommand_posts(
    DBList *ptags,
    DBList *popular_ptags,
    size_t count,
    size_t iteration_i,
    size_t iteration_n);

void basic_aggregate_func(
    DBList *current_ptags,
    UserFeedback *user_feedback,
    size_t iteration_i,
    size_t iteration_n);

// 執行模擬流程
// 為每位使用者推薦指定數量 (posts_recommanded_per_round) 的貼文，並進行指定次數的迭代 (iteration_count)。
// 透過傳入的 recommanded_posts_func 和 aggregate_func 實現推薦與標籤更新邏輯。
void run_simulations(
    size_t posts_recommanded_per_round,
    size_t iteration_count,
    RecommandPostsFunc recommanded_posts_func,
    AggregatePTagsFunc aggregate_func,
    DBList *popular_ptags);

#endif
