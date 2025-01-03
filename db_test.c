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

// ���]�Ҧ��ݭn�����Y�M���c�w�q�A�Ҧp DBList, DBHash, dbobj_create_string_with_dup, ��

// ���ը禡
void test_create_user()
{
  // ��l�Ƹ�Ʈw
  initialize_database();

  // ���ռƾ�
  const char *test_name = "test_user";
  const char *test_tags[] = {"tag1", "tag2", "tag3"};

  // �إ߼��ҦC��
  DBList *tag_list = create_dblist();
  for (int i = 0; i < 3; i++)
  {
    rpush(tag_list, create_dblistnode_with_string(strdup(test_tags[i])));
  }

  // �I�s create_user
  char *user_id = create_user(test_name, tag_list);

  // ���ҵ��G
  if (user_id)
  {
    printf("User ID created: %s\n", user_id);

    // �q�D���ƪ�����Τ�ƾ�
    DBHash *user_data = hget(main_ht, user_id);
    if (user_data)
    {
      // ���ҥΤ�W
      char *stored_name = ht_get_string(user_data, USER_NAME_KEY);
      if (stored_name && strcmp(stored_name, test_name) == 0)
      {
        printf("User name stored correctly: %s\n", stored_name);
      }
      else
      {
        printf("Error: User name not stored correctly.\n");
      }

      // ���Ҽ��ҦC��
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

    // ����O����
    free(user_id);
  }
  else
  {
    printf("Error: create_user returned NULL.\n");
  }

  // ��L�M�z�]�p�G�ݭn�^
}

int main()
{
  test_create_user();
  return 0;
}
