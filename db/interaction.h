#ifndef DB_INTERACTION_H
#define DB_INTERACTION_H

#include "types.h"

DBRequest *create_request(db_action_t action);

DBRequest *reset_request(DBRequest *request, db_action_t action);

void free_request(DBRequest *request);

void free_reply(DBReply *reply);

DBReply *print_reply(DBReply *reply);

DBReply *reply_error(DBReply *reply, const char *message);

DBArg *add_arg_uint(DBRequest *request, db_size_t uint_value);

DBArg *add_arg_string(DBRequest *request, const char *string_value);

DBArg *arg_string_to_uint(DBArg *arg);

#endif