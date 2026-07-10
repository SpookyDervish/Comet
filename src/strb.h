#ifndef STRB_H
#define STRB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} StringBuffer;

void initStringBuff(StringBuffer* sb);
void sbAppend(StringBuffer* sb, const char* fmt, ...);

#endif