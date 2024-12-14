#ifndef DB_API_H
#define DB_API_H

#include "types.h"

bool server_is_running();
void server_config_hash_seed(db_size_t hash_seed);
void server_config_persistence_filepath(const char *persistence_filepath);

void dbapi_start_server();
void dbapi_start_terminal_client();
void dbapi_run_command(const char *command);

DBReply *dbapi_request_async(DBRequest *request);
DBReply *dbapi_request_sync(DBRequest *request);
DBReply *dbapi_await_reply(DBReply *reply);

char *dbapi_get(const char *key);
bool dbapi_set(const char *key, const char *value);
bool dbapi_del(const char *key);
db_size_t dbapi_lpush(const char *key, const char *value);
db_size_t dbapi_lpush_n(const char *key, ...);
char *dbapi_lpop(const char *key);
db_size_t dbapi_rpush(const char *key, const char *value);
db_size_t dbapi_rpush_n(const char *key, ...);
char *dbapi_rpop(const char *key);
db_size_t dbapi_llen(const char *key);
DLList *dbapi_lrange(const char *key, db_size_t start, db_size_t end);
DLList *dbapi_keys();
bool dbapi_shutdown();
bool dbapi_save();
bool dbapi_flushall();

void dbapi_free(char *s);
void dbapi_free_list(DLList *list);

#endif
