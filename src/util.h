#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

char* getFileContents(const char* filename);
char *repl_str(const char *str, const char *from, const char *to);

// Returns a dynamically allocated string containing the target line.
// The caller is responsible for freeing the returned memory.
char *getLineInString(const char *str, int target_line);

// returns a dynamically allocated string containing the repeated string.
// The caller is responsible for freeing the returned memory.
char* repeatString(const char* str, int times);

#endif