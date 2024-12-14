#ifndef DATABASE_H
#define DATABASE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// 定義表示標籤與權重的結構
typedef struct
{
  char tag_id[7]; // 動態字串 (OID)，需以'\0'結尾
  uint8_t weight;
} tag_id_weight_t;

//----------------------------------
// User 相關函式
//----------------------------------

// 取得所有使用者的 id。
// 呼叫者負責釋放傳回的字串陣列。
// 回傳值：包含所有使用者 OID 的字串陣列 (char**)， limit 回傳資料的最大筆數。
char **get_user_ids(size_t limit);

// 計算使用者數量
size_t count_users(void);

// 建立新使用者 (name 為可選)，回傳該使用者的 OID (需自行釋放)
// 若無需 name，可在呼叫時傳入 NULL 或空字串。
char *create_user(const char *name);

// 刪除使用者
bool delete_user(const char *user_id);

// 重新命名使用者
// 需先確認使用者有 name 屬性才呼叫此函式
bool rename_user(const char *user_id, const char *new_name);

// 透過使用者名稱取得使用者 OID (需自行釋放)
// 需先確認使用者有 name 屬性
char *get_user_by_name(const char *name);

// 取得該使用者發佈的所有貼文 OID 清單
// 呼叫者需釋放傳回的陣列
char **get_user_posts(const char *user_id, size_t *out_count);

// 將 post 標記為該 user 已檢視 (同時增加 post 的 viewed 計數)
bool mark_post_as_viewed_by_user(const char *user_id, const char *post_id);

// 將 post 標記為該 user 按讚 (同時增加 post 的 liked 計數)
bool mark_post_as_liked_by_user(const char *user_id, const char *post_id);

//----------------------------------
// Post 相關函式
//----------------------------------

// 取得所有貼文的 OID。呼叫者需釋放回傳的陣列。
char **get_post_ids(size_t *out_count);

// 計算貼文數量
size_t count_posts(void);

// 建立新貼文，必須指定作者 OID 與標籤(含權重)列表
// tags 陣列由呼叫者提供，tags_count 為陣列大小
// 回傳貼文 OID (需自行釋放)
char *create_post(const char *author_id, const tag_id_weight_t *tags, size_t tags_count);

// 刪除貼文
bool delete_post(const char *post_id);

//----------------------------------
// Tag 相關函式
//----------------------------------

// 取得所有標籤的 OID。呼叫者需釋放回傳的陣列。
char **get_tag_ids(size_t *out_count);

// 計算標籤數量
size_t count_tags(void);

// 建立新標籤，回傳該標籤的 OID (需自行釋放)
char *create_tag(const char *text);

// 編輯標籤文字
bool edit_tag(const char *tag_id, const char *new_text);

// 刪除標籤
bool delete_tag(const char *tag_id);

// 增加使用者 predicted_tags 中指定 tag 的權重 (若不存在則新增)
bool user_increase_tag_weight(const char *user_id, const char *tag_id, int increment);

// 減少使用者 predicted_tags 中指定 tag 的權重 (若權重 <= 0 則移除該 tag)
bool user_decrease_tag_weight(const char *user_id, const char *tag_id, int decrement);

// 增加貼文 tags 中指定 tag 的權重
bool post_increase_tag_weight(const char *post_id, const char *tag_id, int increment);

// 減少貼文 tags 中指定 tag 的權重
bool post_decrease_tag_weight(const char *post_id, const char *tag_id, int decrement);

// 根據某個標籤取得貼文清單 (限制回傳數量 limit)，
// 傳回值依該標籤在貼文中的權重由大到小排序
// 呼叫者需釋放傳回的陣列
char **get_posts_by_tag(const char *tag_id, size_t limit, size_t *out_count);

//----------------------------------
// 其他功能
//----------------------------------

// 計算資料集佔用的記憶體大小 (單位可自行決定)
size_t get_dataset_memory_usage(void);

// 清空整個資料庫
void flush_all(void);

#endif // DATABASE_H
