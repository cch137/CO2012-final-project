#ifndef SOCIAL_NETWORK_H
#define SOCIAL_NETWORK_H

#include <stddef.h>
#include <stdbool.h>

#include "db/types.h"

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

// 提供推薦貼文列表
// 根據使用者的預測標籤 (p_tags)，回傳指定數量 (limit) 的貼文 ID 列表。
// p_tags 需由呼叫者負責釋放，limit 為回傳的實際數量。
// iteration_i 表示當前迭代次數，iteration_n 表示總迭代次數。
typedef DBList *(*RecommandPostsFunc)(
    DBList *p_tags,
    size_t limit,
    size_t iteration_i,
    size_t iteration_n);

// 計算新的 p_tags 列表
// 根據貼文的獲贊記錄 (likes_dict)，更新 current_p_tags 並回傳修改後的結果。
// likes_dict 的 key 為貼文 ID，value 為獲贊數 (整數字符串)。
// iteration_i 表示當前迭代次數，iteration_n 表示總迭代次數。
typedef void (*AggregatePTagsFunc)(
    DBList *current_p_tags,
    DBHash *likes_dict,
    size_t iteration_i,
    size_t iteration_n);

// 初始化社群網路系統
// 清空資料庫並產生模擬的假資料。
void init_social_network(void);

// 獲取單一使用者的按贊行為
// 模擬使用者根據 a_tags 的機率對指定貼文 (post_ids) 進行按贊。
// 回傳的 Hash 中，key 為貼文 ID，value 為 "1" (贊) 或 "0" (未贊)。
DBHash *get_user_feedback(const char *user_id, DBList *post_ids);

// 獲取多名使用者的按贊統計
// 模擬所有使用者根據 a_tags 的機率對指定貼文 (post_ids) 進行按贊。
// 回傳的 Hash 中，key 為貼文 ID，value 為總獲贊數。
DBHash *get_popular_feedback(DBList *post_ids);

// 執行模擬流程
// 為每位使用者推薦指定數量 (posts_count) 的貼文，並進行指定次數的迭代 (iteration_count)。
// 透過傳入的 recommanded_posts_func 和 aggregate_func 實現推薦與標籤更新邏輯。
void run_simulations(
    size_t posts_count,
    size_t iteration_count,
    RecommandPostsFunc recommanded_posts_func,
    AggregatePTagsFunc aggregate_func);

#endif
