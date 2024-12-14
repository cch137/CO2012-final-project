#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "utils.h"

void to_uppercase(char *str)
{
  while (*str)
  {
    *str = toupper((unsigned char)*str);
    str++;
  }
}

#define INPUT_STRING_CHUNK_SIZE 8

char *input_string()
{
  size_t buffer_size = INPUT_STRING_CHUNK_SIZE;
  size_t index = 0;
  char *buffer = (char *)malloc(buffer_size * sizeof(char));

  // return NULL if memory allocation fails
  if (!buffer)
    EXIT_ON_MEMORY_ERROR();

  char c;
  // read characters until EOF or newline
  while ((c = (char)fgetc(stdin)) != EOF && c != '\n')
  {
    // check if the buffer needs to be expanded
    if (index >= buffer_size - 1)
    {
      buffer_size += INPUT_STRING_CHUNK_SIZE;
      buffer = (char *)realloc(buffer, buffer_size * sizeof(char));
      if (!buffer)
        EXIT_ON_MEMORY_ERROR();
    }
    // store the character in the buffer
    buffer[index++] = c;
  }

  // if no characters were read, free and return NULL
  if (index == 0)
  {
    free(buffer);
    return NULL;
  }

  buffer[index] = '\0'; // Null-terminate the string

  // reallocate memory to match the exact string length
  buffer = (char *)realloc(buffer, (index + 1) * sizeof(char));
  if (!buffer)
    EXIT_ON_MEMORY_ERROR();

  return buffer; // return the final string
}

char *dbutil_strdup(const char *source)
{
  if (!source)
    return NULL;
  char *dup = (char *)malloc((strlen(source) + 1) * sizeof(char));
  if (!dup)
    EXIT_ON_MEMORY_ERROR();
  strcpy(dup, source);
  return dup;
}

void _exit_on_memory_error(const char *filename, int line, const char *funcname)
{
  printf("Error: Memory allocation failed in '%s' function at %s:%d\n", funcname, filename, line);
  exit(1);
}
