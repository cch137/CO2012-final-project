#ifndef DATABASE_H
#define DATABASE_H

#define POPULAR_USER_NAME "popular"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "db/list.h"

// 取得所有使用者的 id。
// 呼叫者負責釋放傳回的字串陣列。
// 回傳值：包含所有使用者 OID 的字串陣列 (char**)
DBList *get_user_ids();

// 建立新使用者 (name 為可選)，回傳該使用者的 OID (需自行釋放)
// atags 是一個由 string 組成的 List
// s 先不設
// 若無需 name，可在呼叫時傳入 NULL 或空字串。
char *create_user_with_id_returned(const char *name, DBList *atags);

void create_user(const char *name, DBList *atags);

char *get_user_id_by_name(const char *name);

void delete_user(const char *oid);

// 取得所有貼文的 OID。呼叫者需釋放回傳的陣列。
// 呼叫者負責釋放傳回的字串陣列。
DBList *get_post_ids();

// 建立新貼文，必須指定作者 OID 與標籤(含權重)列表
// tags 是一個由 string 組成的 List
// 回傳貼文 OID (需自行釋放)
char *create_post_with_id_returned(DBList *tags);

void create_post(DBList *tags);

void create_post_indexes();

void delete_posts();

// 取得 post 的 tags
// 呼叫者負責釋放傳回的字串陣列。
DBList *get_post_tags(const char *tag_id);

// 根據某個標籤取得貼文清單
// 呼叫者需釋放傳回的陣列
DBList *get_posts_by_tag(const char *tag_id, size_t limit, const bool by_index);

// 取得所有標籤的 OID。呼叫者需釋放回傳的陣列。
// 呼叫者負責釋放傳回的字串陣列。
DBList *get_tag_ids();

// 建立新標籤，回傳該標籤的 OID (需自行釋放)
char *create_tag_with_id_returned(const char *name);

void create_tag(const char *name);

DBList *get_user_atags(const char *user_id);

db_bool_t set_user_atags(const char *user_id, DBList *tags);

DBList *get_user_ptags(const char *user_id);

db_bool_t set_user_ptags(const char *user_id, DBList *tags);

void start_db(void);

void save_db(void);

// 清空整個資料庫
void flush_all(void);

#endif
