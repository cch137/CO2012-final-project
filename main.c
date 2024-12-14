#include "db/api.h"

static inline void init_db_for_test()
{
  dbapi_run_command("FLUSHALL");
  dbapi_run_command("SET author cch");
  dbapi_run_command("SET author cch137");
  dbapi_run_command("SET hw 3");
  dbapi_run_command("SET foo bar");
  dbapi_run_command("DEL foo");
  dbapi_run_command("RPUSH list1 a b c d e f g");
  dbapi_run_command("LPUSH list2 x y z");
  dbapi_run_command("RPOP list1 2");
  dbapi_run_command("LPOP list2 1");
  dbapi_run_command("SAVE");
}

int main()
{
  dbapi_start_server();
  init_db_for_test();
  dbapi_start_terminal_client();
  return 0;
}
