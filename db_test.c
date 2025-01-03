#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ���]���H�U�禡�ŧi�M���c
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

// �����禡��{
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

// ���յ{���J�f
int main()
{
  // �ǳƤ޼�
  const char *name = "example_user";
  DBList *a_tags = create_dblist();
  if (a_tags)
  {
    rpush(a_tags, create_dblistnode_with_string("tag1"));
    rpush(a_tags, create_dblistnode_with_string("tag2"));
    rpush(a_tags, create_dblistnode_with_string("tag3"));
  }

  // �I�s create_user �禡
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

  // �M�z�O����
  // �M�z�C��`�I�P���
  DBListNode *curr = a_tags->head;
  while (curr)
  {
    DBListNode *next = curr->next;
    free(curr->data); // ���] data �O���t���r��
    free(curr);
    curr = next;
  }
  free(a_tags); // �M�z�C����

  return 0;
}
