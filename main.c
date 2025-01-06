#include <stdio.h>

#include "db/api.h"
#include "db/interaction.h"
#include "algorithms.h"
#include "social_network.h"
#include "database.h"

int main()
{
  printf("program start\n");
  dbapi_start_server();

  init_social_network();
  printf("social network inited\n");

  DBList *popular_tags = get_popular_tags();
  printf("calculated popular tags\n");

  dbapi_save();
  printf("database saved to JSON file\n");

  run_simulations(1000, 100, basic_recommand_posts, SQUARE, popular_tags);
  printf("simulations done\n");

  DBListNode *popular_tags_node = popular_tags->head;
  dbapi_set("user:popular:name", "popular");
  while (popular_tags_node)
  {
    dbapi_rpush("user:popular:atags", popular_tags_node->data->value.string);
    popular_tags_node = popular_tags_node->next;
  }
  free_dblist(popular_tags);

  clear_posts();
  printf("posts cleared\n");

  dbapi_save();
  printf("database saved to JSON file\n");

  printf("program end\n");

  return 0;
}
