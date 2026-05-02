#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

char* getFileContents(const char* filename);
char *repl_str(const char *str, const char *from, const char *to);

#endif