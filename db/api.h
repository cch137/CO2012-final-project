#ifndef DB_API_H
#define DB_API_H

#include "types.h"

bool server_is_running();
void server_config_hash_seed(db_size_t hash_seed);
void server_config_persistence_filepath(const char *persistence_filepath);

// Starts the database and sets db_seed to a random number
void dbapi_start_server();

DBReply *dbapi_request_async(DBRequest *request);

DBReply *dbapi_request_sync(DBRequest *request);

DBReply *dbapi_await_reply(DBReply *reply);

void dbapi_start_terminal_client();

void dbapi_run_command(const char *command);

#endif
