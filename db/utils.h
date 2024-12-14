#ifndef DB_UTILS_H
#define DB_UTILS_H

#include <stdbool.h>

// Handles memory allocation errors by printing an error message and exiting the program.
#define EXIT_ON_MEMORY_ERROR() _exit_on_memory_error(__FILE__, __LINE__, __func__)

void to_uppercase(char *str);

char *input_string();

// Duplicates a string, allocating memory for the new string.
char *dbutil_strdup(const char *source);

bool dbutil_match_keys(const char *source, const char *pattern);

// Don't use this function, please use the marco "EXIT_ON_MEMORY_ERROR()".
void _exit_on_memory_error(const char *filename, int line, const char *funcname);

#endif
