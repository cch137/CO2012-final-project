#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "doubly_linked_list.h"
#include "interaction.h"

static DBArg *add_arg(DBRequest *request, db_type_t type);

DBRequest *create_request(db_action_t action)
{
  DBRequest *request = (DBRequest *)calloc(1, sizeof(DBRequest));
  if (!request)
    EXIT_ON_MEMORY_ERROR();
  request->action = action;
  return request;
};

DBRequest *reset_request(DBRequest *request, db_action_t action)
{
  if (!request)
    return NULL;
  request->action = action;
  DBArg *arg = request->arg_head;
  while (arg)
  {
    request->arg_head = arg->next;
    free(arg);
    arg = request->arg_head;
  }
  request->arg_head = NULL;
  request->arg_tail = NULL;
  return request;
};

void free_request(DBRequest *request)
{
  if (!request)
    return;
  DBArg *arg = request->arg_head;
  while (arg)
  {
    request->arg_head = arg->next;
    if (arg->type == DB_TYPE_STRING)
      free(arg->value.string);
    free(arg);
    arg = request->arg_head;
  }
  free(request);
};

void free_reply(DBReply *reply)
{
  if (!reply)
    return;

  switch (reply->type)
  {
  case DB_TYPE_ERROR:
  case DB_TYPE_STRING:
    free(reply->value.string);
    break;
  case DB_TYPE_LIST:
    free_list(reply->value.list);
    break;
  case DB_TYPE_UINT:
  default:
    break;
  }

  free(reply);
}

DBReply *print_reply(DBReply *reply)
{
  if (!reply)
  {
    printf("(nil)\n");
    return reply;
  }

  switch (reply->type)
  {
  case DB_TYPE_NULL:
    printf("(nil)\n");
    break;
  case DB_TYPE_ERROR:
    printf("(error) %s\n", reply->value.string ? reply->value.string : "");
    break;
  case DB_TYPE_STRING:
    printf("%s\n", reply->value.string ? reply->value.string : "");
    break;
  case DB_TYPE_UINT:
    printf("(uint) %lu\n", reply->value.size);
    break;
  case DB_TYPE_LIST:
    printf("(list) count: %u\n", reply->value.list ? reply->value.list->length : 0);
    if (!reply->value.list)
      break;
    db_size_t i = 0;
    DLNode *node = reply->value.list->head;
    while (node)
      printf("  %u) %s\n", ++i, node->data), node = node->next;
    break;
  case DB_TYPE_BOOL:
    printf("(bool) %s\n", reply->ok ? "true" : "false");
    break;
  default:
    printf("(unknown) type=%lu\n", reply->type);
    break;
  }

  return reply;
}

DBReply *reply_error(DBReply *reply, const char *message)
{
  if (!reply)
    return NULL;
  reply->ok = false;
  reply->type = DB_TYPE_ERROR;
  reply->value.string = dbutil_strdup(message);
  return reply;
}

static DBArg *add_arg(DBRequest *request, db_type_t type)
{
  if (!request)
    return NULL;
  DBArg *arg = (DBArg *)calloc(1, sizeof(DBArg));
  if (!arg)
    EXIT_ON_MEMORY_ERROR();
  arg->type = type;
  if (!request->arg_head)
    request->arg_head = arg;
  if (request->arg_tail)
    request->arg_tail->next = arg;
  request->arg_tail = arg;
  return arg;
};

DBArg *add_arg_uint(DBRequest *request, db_size_t value)
{
  if (!request)
    return NULL;
  DBArg *arg = add_arg(request, DB_TYPE_UINT);
  arg->value.size = value;
  return arg;
};

DBArg *add_arg_string(DBRequest *request, const char *value)
{
  if (!request)
    return NULL;
  DBArg *arg = add_arg(request, DB_TYPE_STRING);
  arg->value.string = dbutil_strdup(value);
  return arg;
};

DBArg *arg_string_to_uint(DBArg *arg)
{
  if (arg && arg->type == DB_TYPE_STRING)
  {
    arg->type = DB_TYPE_UINT;
    char *s = arg->value.string;
    if (s)
      arg->value.size = strtoul(s, NULL, 10),
      free(s);
  }
  return arg;
}