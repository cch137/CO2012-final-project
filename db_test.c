#include <stdio.h>
#include <stdlib.h>
#include <string.h>
<<<<<<< HEAD

// 假設有以下函式宣告和結構
typedef struct DBListNode
{
  void *data;
  struct DBListNode *next;
} DBListNode;

typedef struct DBList
{
  DBListNode *head;
  DBListNode *tail;
} DBList;

DBList *create_dblist();
DBListNode *create_dblistnode_with_string(const char *str);
void rpush(DBList *list, DBListNode *node);
char *create_user(const char *name, DBList *a_tags);

// 模擬函式實現
DBList *create_dblist()
{
  DBList *list = (DBList *)malloc(sizeof(DBList));
  if (!list)
    return NULL;
  list->head = NULL;
  list->tail = NULL;
  return list;
}

DBListNode *create_dblistnode_with_string(const char *str)
{
  DBListNode *node = (DBListNode *)malloc(sizeof(DBListNode));
  if (!node)
    return NULL;
  node->data = strdup(str);
  node->next = NULL;
  return node;
}

void rpush(DBList *list, DBListNode *node)
{
  if (!list || !node)
    return;
  if (!list->head)
  {
    list->head = node;
    list->tail = node;
  }
  else
  {
    list->tail->next = node;
    list->tail = node;
  }
}

// 測試程式入口
int main()
{
  // 準備引數
  const char *name = "example_user";
  DBList *a_tags = create_dblist();
  if (a_tags)
  {
    rpush(a_tags, create_dblistnode_with_string("tag1"));
    rpush(a_tags, create_dblistnode_with_string("tag2"));
    rpush(a_tags, create_dblistnode_with_string("tag3"));
  }

  // 呼叫 create_user 函式
  char *user_id = create_user(name, a_tags);
  if (user_id)
  {
    printf("User created with ID: %s\n", user_id);
    free(user_id);
  }
  else
  {
    printf("Failed to create user.\n");
  }

  // 清理記憶體
  // 清理列表節點與資料
  DBListNode *curr = a_tags->head;
  while (curr)
  {
    DBListNode *next = curr->next;
    free(curr->data); // 假設 data 是分配的字串
    free(curr);
    curr = next;
  }
  free(a_tags); // 清理列表本身
=======
#include "database.h"

#define USER_NS_PREFIX "user:"
#define POST_NS_PREFIX "post:"
#define TAG_NS_PREFIX "tag:"
#define USER_NAME_KEY "name"
#define USER_ATAGS_KEY "a_tags"
#define USER_NS_PREFIX "user:"

DBHash *main_ht = NULL;
DBHash *expr_ht = NULL;

// 測試結果輸出
void assert_test(const char *test_name, int condition)
{
  if (condition)
  {
    printf("[PASS] %s\n", test_name);
  }
  else
  {
    printf("[FAIL] %s\n", test_name);
  }
}

// 測試 create_user 函式
void test_create_user()
{
  // 測試 1: 創建帶有 name 和 a_tags 的使用者
  DBList *a_tags1 = create_dblist();
  rpush(a_tags1, create_dblistnode_with_string("tag1"));
  rpush(a_tags1, create_dblistnode_with_string("tag2"));

  char *user_id1 = create_user("Alice", a_tags1);

  assert_test("Test 1: User ID is not NULL", user_id1 != NULL);

  // 驗證使用者是否成功添加到 main_ht
  DBHashEntry *user_entry1 = hget(main_ht, user_id1, expr_ht);
  assert_test("Test 1: User entry exists in main_ht", user_entry1 != NULL);

  if (user_entry1 && dbobj_is_hash(user_entry1->data))
  {
    DBHash *user_data1 = user_entry1->data->value.hash;

    // 驗證 name
    DBHashEntry *name_entry = hget(user_data1, USER_NAME_KEY, NULL);
    assert_test("Test 1: User name exists", name_entry != NULL);
    if (name_entry)
    {
      assert_test("Test 1: User name is correct", strcmp(name_entry->data->value.string, "Alice") == 0);
    }

    // 驗證 a_tags
    DBHashEntry *atags_entry = hget(user_data1, USER_ATAGS_KEY, NULL);
    assert_test("Test 1: User a_tags exists", atags_entry != NULL);
    if (atags_entry)
    {
      DBList *tags = atags_entry->data->value.list;
      assert_test("Test 1: User a_tags length is correct", tags->length == 2);
    }
  }

  free(user_id1);
  free_dblist(a_tags1);

  // 測試 2: 創建無 name 的使用者
  char *user_id2 = create_user(NULL, NULL);
  assert_test("Test 2: User ID is not NULL", user_id2 != NULL);

  // 驗證使用者是否成功添加到 main_ht
  DBHashEntry *user_entry2 = hget(main_ht, user_id2, expr_ht);
  assert_test("Test 2: User entry exists in main_ht", user_entry2 != NULL);

  if (user_entry2 && dbobj_is_hash(user_entry2->data))
  {
    DBHash *user_data2 = user_entry2->data->value.hash;

    // 驗證 name 不存在
    DBHashEntry *name_entry2 = hget(user_data2, USER_NAME_KEY, NULL);
    assert_test("Test 2: User name does not exist", name_entry2 == NULL);

    // 驗證 a_tags 不存在
    DBHashEntry *atags_entry2 = hget(user_data2, USER_ATAGS_KEY, NULL);
    assert_test("Test 2: User a_tags does not exist", atags_entry2 == NULL);
  }

  free(user_id2);

  // 測試 3: 創建帶空 name 的使用者
  char *user_id3 = create_user("", NULL);
  assert_test("Test 3: User ID is not NULL", user_id3 != NULL);

  // 驗證使用者是否成功添加到 main_ht
  DBHashEntry *user_entry3 = hget(main_ht, user_id3, expr_ht);
  assert_test("Test 3: User entry exists in main_ht", user_entry3 != NULL);

  if (user_entry3 && dbobj_is_hash(user_entry3->data))
  {
    DBHash *user_data3 = user_entry3->data->value.hash;

    // 驗證 name 不存在
    DBHashEntry *name_entry3 = hget(user_data3, USER_NAME_KEY, NULL);
    assert_test("Test 3: User name does not exist", name_entry3 == NULL);
  }

  free(user_id3);

  // 測試 4: 創建帶空 a_tags 的使用者
  DBList *a_tags4 = create_dblist();
  char *user_id4 = create_user("Bob", a_tags4);
  assert_test("Test 4: User ID is not NULL", user_id4 != NULL);

  // 驗證使用者是否成功添加到 main_ht
  DBHashEntry *user_entry4 = hget(main_ht, user_id4, expr_ht);
  assert_test("Test 4: User entry exists in main_ht", user_entry4 != NULL);

  if (user_entry4 && dbobj_is_hash(user_entry4->data))
  {
    DBHash *user_data4 = user_entry4->data->value.hash;

    // 驗證 name
    DBHashEntry *name_entry4 = hget(user_data4, USER_NAME_KEY, NULL);
    assert_test("Test 4: User name exists", name_entry4 != NULL);
    if (name_entry4)
    {
      assert_test("Test 4: User name is correct", strcmp(name_entry4->data->value.string, "Bob") == 0);
    }

    // 驗證 a_tags 不存在
    DBHashEntry *atags_entry4 = hget(user_data4, USER_ATAGS_KEY, NULL);
    assert_test("Test 4: User a_tags does not exist", atags_entry4 == NULL);
  }

  free(user_id4);
  free_dblist(a_tags4);

  printf("All tests completed.\n");
}
>>>>>>> da2a4e835a17b7f053bf24022025ecfc41aa7da9

  return 0;
}
