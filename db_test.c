#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  return 0;
}
