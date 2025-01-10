#include <stdio.h>

#include "algorithms.h"
#include "social_network.h"
#include "database.h"
#include "db/api.h"

int main()
{
  printf("program start\n");
  start_db();

  init_social_network();
  printf("social network inited\n");

  UserFeedback *popular_feedback = collect_popular_feedback(NULL);
  printf("calculated popular tags\n");

  // init_users_ptags(NULL);

  AlgoFunction r_alogs[] = {s_075_025, s_cube, s_selu, s_sigmoid, s_square, s_create};
  AlgoFunction a_alogs[] = {s_075_025, s_cube, s_selu, s_sigmoid, s_square, s_create};

  // for (int r = 0; r < 6; ++r)
  // {
  //   for (int a = 0; a < 6; ++a)
  //   {
  //     for (int t = 0; t < 1; ++t)
  //     {
  //       // printf("SIMULATIONS R%dA%d test%d\n", r + 1, a + 1, t + 1);
  //       printf("R%dA%d\t", r + 1, a + 1);
  //       fflush(stdout);
  //       init_users_ptags(NULL);
  //       run_simulations(
  //           20, 20,
  //           r_alogs[r],
  //           a_alogs[a],
  //           NULL);

  //       save_db();
  //       // printf("database saved to JSON file\n");

  //       system("python analysis.py");
  //       // printf("analysis data to JSON file\n");
  //     }
  //   }
  // }

  // for (int i = 1; i <= 500;)
  // {
  //   init_users_ptags(NULL);
  //   printf("%d\t", i);
  //   fflush(stdout);
  //   run_simulations(
  //       10, i,
  //       s_cube,
  //       s_075_025,
  //       NULL);
  //   save_db();
  //   system("python analysis.py");
  //   if (i < 5)
  //     ++i;
  //   else if (i < 10)
  //     i += 5;
  //   else if (i < 100)
  //     i += 10;
  //   else
  //     i += 50;
  // }

  printf("---\n");

  init_users_ptags(NULL);
  fflush(stdout);
  run_simulations(
      10, 10,
      s_square,
      s_075_025,
      NULL);
  save_db();
  system("python analysis.py");

  printf("---\n");

  init_users_ptags(popular_feedback->ptags);
  fflush(stdout);
  run_simulations(
      10, 10,
      s_square,
      s_075_025,
      NULL);
  save_db();
  system("python analysis.py");

  // printf("simulations done\n");
  // delete_posts();
  // printf("posts cleared\n");

  free_user_feedback(popular_feedback);
  printf("program end\n");

  return 0;
}
