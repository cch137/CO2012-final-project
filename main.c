#include <stdio.h>

#include "algorithms.h"
#include "social_network.h"
#include "database.h"

int main()
{
  printf("program start\n");
  start_db();

  init_social_network();
  printf("social network inited\n");

  UserFeedback *popular_feedback = get_popular_feedback(NULL);
  printf("calculated popular tags\n");

  run_simulations(
      20, 25,
      basic_recommand_posts,
      basic_aggregate_func,
      NULL);
  printf("simulations done\n");

  clear_posts();
  printf("posts cleared\n");

  save_db();
  printf("database saved to JSON file\n");

  system("python analysis.py");
  printf("analysis data to JSON file\n");

  free_user_feedback(popular_feedback);
  printf("program end\n");

  return 0;
}
