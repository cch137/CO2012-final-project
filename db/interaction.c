#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "obj.h"
#include "list.h"
#include "interaction.h"

DBRequest *create_request(db_action_t action)
{
  DBRequest *request = (DBRequest *)malloc(sizeof(DBRequest));
  if (!request)
    EXIT_ON_MEMORY_ERROR();
  request->action = action;
  request->args = NULL;
  return request;
};

DBReply *create_reply()
{
  DBReply *reply = (DBReply *)malloc(sizeof(DBReply));
  if (!reply)
    EXIT_ON_MEMORY_ERROR();
  reply->done = false;
  reply->data = NULL;
  return reply;
};

void add_request_arg(DBRequest *request, DBObj *arg)
{
  if (!request)
    return;
  if (!request->args)
    request->args = create_dblist();
  DBListNode *node = create_dblistnode(arg);
  if (arg)
  {
    join_dblistnodes(request->args->tail, node);
    if (!request->args->head)
      request->args->head = node;
    request->args->tail = node;
    request->args->length++;
    rpush;
  }
};

DBRequest *reset_request(DBRequest *request, db_action_t action)
{
  if (!request)
    return NULL;
  request->action = action;
  if (request->args)
  {
    free_dblist(request->args);
    request->args = NULL;
  }
  return request;
};

void free_request(DBRequest *request)
{
  if (!request)
    return;

  free_dblist(request->args);
  free(request);
};

void free_reply(DBReply *reply)
{
  if (!reply)
    return;

  free_dbobj(reply->data);
  free(reply);
}

DBReply *print_reply(DBReply *reply)
{
  if (!reply)
  {
    printf("(nil)\n");
    return reply;
  }

  switch (reply->data->type)
  {
  case DB_TYPE_NULL:
    printf("(nil)\n");
    break;
  case DB_TYPE_ERROR:
    printf("(error) %s\n", reply->data->value.string ? reply->data->value.string : "");
    break;
  case DB_TYPE_BOOL:
    printf("(db_bool_t) %s\n", reply->data->value.bool_value ? "true" : "false");
    break;
  case DB_TYPE_INT:
    printf("(int) %lu\n", reply->data->value.uint_value);
    break;
  case DB_TYPE_UINT:
    printf("(uint) %lu\n", reply->data->value.uint_value);
    break;
  case DB_TYPE_STRING:
    printf("%s\n", reply->data->value.string ? reply->data->value.string : "");
    break;
  case DB_TYPE_LIST:
    printf("(list) count: %u\n", reply->data->value.list ? reply->data->value.list->length : 0);
    if (!reply->data->value.list)
    {
      printf("Unexpected empty pointer\n");
      break;
    }
    db_uint_t i = 0;
    DBListNode *node = reply->data->value.list->head;
    while (node)
      printf("  %u) %s\n", ++i, node->data->value.string), node = node->next;
    break;
  default:
    printf("(unknown) type=%lu\n", reply->data->type);
    break;
  }

  return reply;
}

DBReply *reply_error(DBReply *reply, const char *message)
{
  if (!reply)
    return NULL;
  if (reply->data)
    free_dbobj(reply->data);
  reply->data = dbobj_create_error(dbutil_strdup(message));
  return reply;
}

DBReply *reply_data(DBReply *reply, DBObj *data)
{
  if (!reply)
    return NULL;
  if (reply->data)
    free_dbobj(reply->data);
  reply->data = data;
  return reply;
}

char *get_string_arg(DBListNode *curr_node)
{
  if (!curr_node || !curr_node->data || !dbobj_is_string(curr_node->data))
    return NULL;
  return curr_node->data->value.string;
}

db_uint_t get_uint_arg(DBListNode *curr_node)
{
  if (!curr_node || !curr_node->data)
    return 0;
  if (dbobj_is_string(curr_node->data))
    arg_string_to_uint(curr_node->data);
  if (dbobj_is_uint(curr_node->data))
    return curr_node->data->value.uint_value;
  return 0;
}

db_int_t get_int_arg(DBListNode *curr_node)
{
  if (!curr_node || !curr_node->data)
    return 0;
  if (dbobj_is_string(curr_node->data))
    arg_string_to_int(curr_node->data);
  if (dbobj_is_int(curr_node->data))
    return curr_node->data->value.int_value;
  return 0;
}

DBObj *arg_string_to_uint(DBObj *obj)
{
  if (!obj || obj->type != DB_TYPE_STRING)
    return obj;

  char *s = obj->value.string;
  if (s)
  {
    obj->type = DB_TYPE_UINT;
    obj->value.uint_value = (db_uint_t)strtoul(s, NULL, 10),
    free(s);
  }

  return obj;
}

DBObj *arg_string_to_int(DBObj *obj)
{
  if (!obj || obj->type != DB_TYPE_STRING)
    return obj;

  char *s = obj->value.string;
  if (s)
  {
    obj->type = DB_TYPE_INT;
    obj->value.int_value = (db_int_t)strtol(s, NULL, 10),
    free(s);
  }

  return obj;
}
