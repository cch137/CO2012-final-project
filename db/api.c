#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "interaction.h"
#include "core.h"
#include "api.h"

// Parses command string into DBRequest structure
static DBRequest *parse_command(const char *command);

bool server_is_running()
{
  core_lock();
  bool _is_running = db_is_running();
  core_unlock();
  return _is_running;
}

void server_config_hash_seed(db_size_t hash_seed)
{
  core_lock();
  db_config_hash_seed(hash_seed);
  core_unlock();
}

void server_config_persistence_filepath(const char *persistence_filepath)
{
  core_lock();
  db_config_persistence_filepath(persistence_filepath);
  core_unlock();
}

void dbapi_start_server()
{
  core_lock();
  db_start();
  core_unlock();
}

DBReply *dbapi_request_async(DBRequest *request)
{
  if (!request)
    return NULL;

  core_lock();
  DBReply *reply = db_handle_request(request);
  core_unlock();

  return reply;
};

DBReply *dbapi_request_sync(DBRequest *request)
{
  return dbapi_await_reply(dbapi_request_async(request));
};

DBReply *dbapi_await_reply(DBReply *reply)
{
  if (!reply)
    return NULL;

  while (true)
  {
    core_lock();
    if (reply->done)
    {
      core_unlock();
      break;
    }
    core_unlock();
  }

  return reply;
};

void dbapi_start_terminal_client()
{
  char *command_buffer = NULL;
  DBRequest *request = NULL;
  DBRequest *reply = NULL;

  dbapi_start_server();
  printf("Welcome to cch137's database!\n");
  printf("Please use commands to interact with the database.\n");

  while (server_is_running())
  {
    printf("> ");
    command_buffer = input_string();
    if (!command_buffer)
      continue;
    request = parse_command(command_buffer);
    free_reply(print_reply(dbapi_request_sync(request)));
    free_request(request);
    free(command_buffer);
  }

  printf("Goodbye! The database has been shut down.\n");
}

void dbapi_run_command(const char *command)
{
  DBRequest *request = parse_command(command);
  free_reply(dbapi_request_sync(request));
  free_request(request);
}

static DBRequest *parse_command(const char *command)
{
  if (!command)
    return NULL;

  DBRequest *request = (DBRequest *)calloc(1, sizeof(DBRequest));
  if (!request)
    EXIT_ON_MEMORY_ERROR();

  // Duplicate command for tokenization
  char *command_copy = dbutil_strdup(command);
  if (!command_copy)
    EXIT_ON_MEMORY_ERROR();

  char *token = strtok(command_copy, " ");

  // Parse action string into db_action_t
  to_uppercase(token);
  if (!token)
    request->action = DB_UNKNOWN_COMMAND;
  if (strcmp(token, "SAVE") == 0)
    request->action = DB_SAVE;
  else if (strcmp(token, "START") == 0)
    request->action = DB_START;
  else if (strcmp(token, "SET") == 0)
    request->action = DB_SET;
  else if (strcmp(token, "GET") == 0)
    request->action = DB_GET;
  else if (strcmp(token, "RENAME") == 0)
    request->action = DB_RENAME;
  else if (strcmp(token, "DEL") == 0)
    request->action = DB_DEL;
  else if (strcmp(token, "LPUSH") == 0)
    request->action = DB_LPUSH;
  else if (strcmp(token, "LPOP") == 0)
    request->action = DB_LPOP;
  else if (strcmp(token, "RPUSH") == 0)
    request->action = DB_RPUSH;
  else if (strcmp(token, "RPOP") == 0)
    request->action = DB_RPOP;
  else if (strcmp(token, "LLEN") == 0)
    request->action = DB_LLEN;
  else if (strcmp(token, "LRANGE") == 0)
    request->action = DB_LRANGE;
  else if (strcmp(token, "KEYS") == 0)
    request->action = DB_KEYS;
  else if (strcmp(token, "FLUSHALL") == 0)
    request->action = DB_FLUSHALL;
  else if (strcmp(token, "INFO_DATASET_MEMORY") == 0)
    request->action = DB_INFO_DATASET_MEMORY;
  else if (strcmp(token, "SHUTDOWN") == 0)
    request->action = DB_SHUTDOWN;
  else
    request->action = DB_UNKNOWN_COMMAND;

  // Move past action in original command string
  const char *pos = command + strlen(token);

  while (*pos != '\0')
  {
    // Skip extra whitespace
    while (isspace(*pos))
      ++pos;

    if (*pos == '\0')
      break;

    DBArg *arg = NULL;
    if (*pos == '"')
    {
      // Parse quoted string
      ++pos;
      const char *start = pos;
      size_t length = 0;
      char *string_value = NULL;

      // Continue until end of string or until reaching an unescaped quote
      while (*pos != '\0' && (*pos != '"' || (*(pos - 1) == '\\' && pos != start)))
      {
        // Handle escape
        if (*pos == '\\' && *(pos + 1) == '"')
          ++pos;
        ++pos;
        ++length;
      }

      if (*pos == '"')
      {
        string_value = (char *)malloc(length + 1);
        if (!string_value)
          EXIT_ON_MEMORY_ERROR();

        // Remove escape sequences
        size_t i = 0;
        const char *src = start;
        while (src < pos)
        {
          if (*src == '\\' && *(src + 1) == '"')
          {
            string_value[i++] = '"';
            src += 2;
          }
          else
          {
            string_value[i++] = *src++;
          }
        }
        string_value[i] = '\0';
        ++pos;

        arg = add_arg_string(request, string_value);
        free(string_value);
      }
    }
    else
    {
      // Parse signed or unsigned integer, or treat as a string
      char *endptr = NULL;
      const char *start = pos;
      while (*pos != '\0' && !isspace(*pos))
        ++pos;
      size_t length = pos - start;

      char *string_value = (char *)malloc(length + 1);
      if (!string_value)
        EXIT_ON_MEMORY_ERROR();

      strncpy(string_value, start, length);
      string_value[length] = '\0';

      arg = add_arg_string(request, string_value);
      free(string_value);
    }

    if (!arg)
      EXIT_ON_MEMORY_ERROR();
  }

  free(command_copy);
  return request;
}
