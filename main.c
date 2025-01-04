#include "db/api.h"
#include "social_network.h"

int main()
{
  dbapi_start_server();

  init_social_network();

  // 演算法評估結果 = run_simulations(1000, 100, recommand, ori_design_aggregate);
  // 演算法評估結果 --> DB

  // 重置 user 的 ptags

  // 演算法評估結果 = run_simulations(1000, 100, recommand, 演算法 二);
  // 演算法評估結果 --> DB

  // 重置 user 的 ptags

  // 演算法評估結果 = run_simulations(1000, 100, recommand, 演算法 三);
  // 演算法評估結果 --> DB

  dbapi_start_terminal_client();

  return 0;
}
