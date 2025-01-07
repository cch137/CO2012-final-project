#ifndef DB_API_H
#define DB_API_H

#include "types.h"

db_bool_t server_is_running();
void server_config_hash_seed(db_uint_t hash_seed);
void server_config_persistence_filepath(const char *persistence_filepath);

void dbapi_start_server();
void dbapi_start_terminal_client();
void dbapi_run_command(const char *command);

DBReply *dbapi_request_async(DBRequest *request);
DBReply *dbapi_request_sync(DBRequest *request);
DBReply *dbapi_await_reply(DBReply *reply);

char *dbapi_get(const char *key);
db_bool_t dbapi_set(const char *key, const char *value);
db_uint_t dbapi_del(const char *key);
db_bool_t dbapi_rename(const char *old_key, const char *new_key);
db_uint_t dbapi_lpush(const char *key, const char *value);
db_uint_t dbapi_lpush_n(const char *key, ...);
char *dbapi_lpop(const char *key);
db_uint_t dbapi_rpush(const char *key, const char *value);
db_uint_t dbapi_rpush_n(const char *key, ...);
char *dbapi_rpop(const char *key);
db_uint_t dbapi_llen(const char *key);
DBList *dbapi_lrange(const char *key, db_uint_t start, db_uint_t end);
char *dbapi_hget(const char *key, const char *field);
db_uint_t dbapi_hset(const char *key, const char *field, const char *value);
db_uint_t dbapi_hdel(const char *key, const char *field);
db_int_t dbapi_hincrby(const char *key, const char *field, db_int_t value);
DBList *dbapi_keys();
DBList *dbapi_match_keys(const char *pattern);
db_bool_t dbapi_shutdown();
db_bool_t dbapi_save();
db_bool_t dbapi_flushall();

void dbapi_free(char *s);
void dbapi_free_list(DBList *list);

#endif
