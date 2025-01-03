#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <hiredis/hiredis.h>
#include <ctype.h>

#include "db/utils.h"
#include "db/api.h"

#define USER_NS_PREFIX "user:"
#define POST_NS_PREFIX "post:"
#define TAG_NS_PREFIX "tag:"
#define USER_NAME_KEY "name"
#define USER_ATAGS_KEY "a_tags"
#define USER_NS_PREFIX "user:"

DBHash *main_ht = NULL;
DBHash *expr_ht = NULL;

// 假設所有需要的標頭和結構定義，例如 DBList, DBHash, dbobj_create_string_with_dup, 等

// 測試函式
void test_create_user()
{
  // 初始化資料庫
  initialize_database();

  // 測試數據
  const char *test_name = "test_user";
  const char *test_tags[] = {"tag1", "tag2", "tag3"};

  // 建立標籤列表
  DBList *tag_list = create_dblist();
  for (int i = 0; i < 3; i++)
  {
    rpush(tag_list, create_dblistnode_with_string(strdup(test_tags[i])));
  }

  // 呼叫 create_user
  char *user_id = create_user(test_name, tag_list);

  // 驗證結果
  if (user_id)
  {
    printf("User ID created: %s\n", user_id);

    // 從主哈希表中獲取用戶數據
    DBHash *user_data = hget(main_ht, user_id);
    if (user_data)
    {
      // 驗證用戶名
      char *stored_name = ht_get_string(user_data, USER_NAME_KEY);
      if (stored_name && strcmp(stored_name, test_name) == 0)
      {
        printf("User name stored correctly: %s\n", stored_name);
      }
      else
      {
        printf("Error: User name not stored correctly.\n");
      }

      // 驗證標籤列表
      DBList *stored_tags = ht_get_list(user_data, USER_ATAGS_KEY);
      if (stored_tags)
      {
        printf("Stored tags:\n");
        DBListNode *curr = stored_tags->head;
        int index = 0;
        while (curr)
        {
          printf("  - %s\n", curr->data->value.string);
          if (strcmp(curr->data->value.string, test_tags[index]) != 0)
          {
            printf("Error: Tag mismatch at index %d.\n", index);
          }
          curr = curr->next;
          index++;
        }
      }
      else
      {
        printf("Error: Tags not stored correctly.\n");
      }
    }
    else
    {
      printf("Error: User data not found for ID: %s\n", user_id);
    }

    // 釋放記憶體
    free(user_id);
  }
  else
  {
    printf("Error: create_user returned NULL.\n");
  }

  // 其他清理（如果需要）
}

int main()
{
  test_create_user();
  return 0;
}
