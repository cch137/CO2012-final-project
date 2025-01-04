// test.c
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "db/utils.h"
#include "database.h"

#define RESULT_PASS "\033[0;32mPASS\033[0m"
#define RESULT_FAIL "\033[0;31mFAIL\033[0m"

typedef struct
{
  const char *source;
  const char *pattern;
  db_bool_t expected;
} TestCase;

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

int main()
{
  printf("Tests start.\n");

  test_dbutil_match_keys();

  printf("Tests done!\n");

  return 0;
}
