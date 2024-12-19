#include <stddef.h>
#include <stdbool.h>

#include "db/list.h"
#include "db/utils.h"
#include "social_network.h"
#include "database.h"

static DBList *convert_list_with_string_array(char **array, size_t count)
{
  DBList *list = create_list();
  for (size_t i = 0; i < count; i++)
  {
    rpush(list, create_dblistnode_with_string(array[i]));
    free(array[i]);
  }
  free(array);
  return list;
}

bool init_social_network(void)
{
  return true;
}

DBList *get_random_posts(size_t limit, size_t *out_count)
{
  char **list = get_post_ids(count_posts());
  return NULL;
}

DBList *get_popular_posts(size_t limit, size_t *out_count)
{
  return NULL;
}

DBList *get_popular_tags(size_t limit, size_t *out_count)
{
  return NULL;
}

DBList *get_recommended_posts_for_user(const char *user_id, size_t limit, size_t *out_count)
{
  return NULL;
}

bool user_view_post(const char *user_id, const char *post_id)
{
  return true;
}

bool simulate_user_interaction(const char *user_id, const DBList *post_ids, size_t post_count)
{
  return true;
}

bool run_simulation(void)
{
  return true;
}

double compare_predicted_and_actual_tags(const char *user_id)
{
  return 0;
}
