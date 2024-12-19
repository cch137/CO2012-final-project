#ifndef SOCIAL_NETWORK_H
#define SOCIAL_NETWORK_H

#include <stddef.h>
#include <stdbool.h>

#include "db/types.h"

// 初始化整個社群網路系統
// 此函式將清空資料庫 (呼叫 flush_all) 並產生假資料
// by CCH
bool init_social_network(void);

// 取得隨機貼文列表 (limit)，返回陣列由函式動態配置，呼叫者須釋放
// *out_count 回傳實際取得的貼文數量，不大於 limit
// by Sin
DBList *get_random_posts(size_t limit, size_t *out_count);

// 取得熱門貼文列表 (limit)，可依據 viewed 或 liked 數排序
// 返回陣列需呼叫者釋放，*out_count 回傳實際數量
// by CCH
DBList *get_popular_posts(size_t limit, size_t *out_count);

// 取得熱門標籤列表 (limit)
// 返回陣列需呼叫者釋放，*out_count 回傳實際數量
// by Sin
DBList *get_popular_tags(size_t limit, size_t *out_count);

// 根據使用者的預測標籤給予推薦貼文 (limit)
// 返回陣列需呼叫者釋放，*out_count 回傳實際數量
// by CCH
DBList *get_recommended_posts_for_user(const char *user_id, size_t limit, size_t *out_count);

// 使用者檢視貼文，內部可能同時處理 viewed 和 liked 的機率行為
// by Sin
bool user_view_post(const char *user_id, const char *post_id);

// 模擬使用者對一組貼文進行互動的行為 (view/like)
// post_ids 由呼叫者提供，大小為 post_count
// by CCH
bool simulate_user_interaction(const char *user_id, const DBList *post_ids, size_t post_count);

// 讓所有使用者各進行一次 simulate_user_interaction，以模擬整體社群行為
// by Sin
bool run_simulation(void);

// 比較使用者的 predicted_tags 與 actual_tags 並給出評分
// 如使用 Jaccard、F1 等指標，回傳計算後的分數
// by CCH
double compare_predicted_and_actual_tags(const char *user_id);

#endif // SOCIAL_NETWORK_H
