#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"

#define RESULT_PASS "\033[0;32mPASS\033[0m"
#define RESULT_FAIL "\033[0;31mFAIL\033[0m"

typedef struct
{
  const char *source;
  const char *pattern;
  db_bool_t expected;
} TestCase;

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

db_bool_t dbutil_match_keys(const char *source, const char *pattern)
{
  const char *src_ptr = source;
  const char *pat_ptr = pattern;
  const char *last_star = NULL;
  const char *star_match_pos = source;

  while (*src_ptr)
  {
    if (*pat_ptr == '\\' && *(pat_ptr + 1) != '\0')
    {
      pat_ptr++;
      if (*pat_ptr == *src_ptr)
      {
        pat_ptr++;
        src_ptr++;
      }
      else if (last_star)
      {
        pat_ptr = last_star + 1;
        src_ptr = ++star_match_pos;
      }
      else
      {
        return false;
      }
    }
    else if (*pat_ptr == '*')
    {
      last_star = pat_ptr++;
      star_match_pos = src_ptr;
    }
    else if (*pat_ptr == '?')
    {
      pat_ptr++;
      if (*src_ptr == '\0')
        return false;
      src_ptr++;
    }
    else if (*pat_ptr == *src_ptr)
    {
      pat_ptr++;
      src_ptr++;
    }
    else if (last_star)
    {
      pat_ptr = last_star + 1;
      src_ptr = ++star_match_pos;
    }
    else
    {
      return false;
    }
  }

  while (*pat_ptr == '*')
  {
    pat_ptr++;
  }

  return *pat_ptr == '\0';
}

void debug_print(const char *s)
{
  printf("%s", s);
}

void _exit_on_error(const char *message, const char *filename, int line, const char *funcname)
{
  printf("Error: %s in '%s' function at %s:%d\n", message, funcname, filename, line);
  exit(1);
}

void _exit_on_memory_error(const char *filename, int line, const char *funcname)
{
  printf("Error: Memory allocation failed in '%s' function at %s:%d\n", funcname, filename, line);
  exit(1);
}

void test_dbutil_match_keys()
{
  TestCase test_cases[] = {
      {"user:123", "user:*", true},
      {"user:123", "user:?23", true},
      {"user:abc", "user:abc", true},
      {"user:123", "user:1*3", true},
      {"user:xyz", "user:?yz", true},
      {"user:123", "user:123", true},
      {"user:123", "user:12\\3", true},
      {"user:*23", "user:\\*23", true},
      {"user:abc", "admin:*", false},
      {"user:abc", "user:\\?bc", false},
      {"user:abc", "user:a?c", true},
      {"user:abc", "user:a*c", true},
      {"user:abc", "user:*b*", true},
      {"user:abc", "user:??c", true},
      {"user:abc", "*", true},
      {"", "*", true},
      {"", "?", false},
      {"", "", true},
      {"abc", "a\\*c", false},
      {"a*c", "a\\*c", true},
      {"abc", "???", true},
      {"ab", "???", false},
      {"abcd", "a*d", true},
      {"abc", "a\\?c", false},
      {"a?c", "a\\?c", true},
      {"a*c", "a??c", false},
      {"abbbbc", "a*b*c", true},
      {"abbbbc", "a*c*b", false},
      {"abc", "abc\\", false},
      {"abc", "abc\\d", false},
      {"user:??x", "user:??x", true},
      {"user:?x", "user:??x", false},
      {"hello", "h*llo", true},
      {"heeeello", "h*llo", true},
      {"hey", "h*llo", false},
  };

  size_t test_count = sizeof(test_cases) / sizeof(TestCase);

  for (size_t i = 0; i < test_count; ++i)
  {
    db_bool_t result = dbutil_match_keys(test_cases[i].source, test_cases[i].pattern);
    printf("[%s] Source: \"%s\", Pattern: \"%s\" (Expected: %s)\n",
           (result == test_cases[i].expected) ? RESULT_PASS : RESULT_FAIL,
           test_cases[i].source, test_cases[i].pattern,
           test_cases[i].expected ? "true" : "false");
  }
}