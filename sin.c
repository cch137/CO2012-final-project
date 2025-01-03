#include <stddef.h>
#include <stdbool.h>

#include "db/list.h"
#include "db/utils.h"
#include "social_network.h"
#include "database.h"

// 根據獲贊數記錄計算一組 p_tags List
DBList *calculate_p_tags(const DBHash *liked_dict, const size_t user_count);

// 對 current_p_tags 和 offset_p_tags 根據權重做合併計算
// function 直接修改 current_p_tags 並回傳 current_p_tags
DBList *aggregate_p_tags(
    DBList *current_p_tags,
    const DBHash *liked_dict,
    const size_t user_count,
    const size_t iteration_i,
    const size_t iteration_n);

// 改善舊與新的p_tag怎麼調整
void adjust_propotion_p_tag(DBList *current_p_tags);

// 將p_tag裡面微不足道的權重剪掉
void cut_weak_p_tag(DBList *current_p_tags);

// 權重的總和回到1
void repair_propotion_p_tag(DBList *current_p_tags);

// 對所有使用者進行 1 個回合的預測
// posts_count 是決定投餵給每個使用者的帖文數量
void run_simulation(size_t posts_count);