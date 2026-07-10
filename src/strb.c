#include "strb.h"

void initStringBuff(StringBuffer* sb) {
    sb->capacity = 128;
    sb->length = 0;
    sb->data = (char*)malloc(sb->capacity);
    sb->data[0] = '\0';
}

void sbAppend(StringBuffer* sb, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    // Check how much space we need
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return;
    }

    // Grow buffer if needed
    if (sb->length + needed >= sb->capacity) {
        while (sb->length + needed >= sb->capacity) {
            sb->capacity *= 2;
        }
        sb->data = (char*)realloc(sb->data, sb->capacity);
    }

    // Append the string safely
    vsnprintf(sb->data + sb->length, needed + 1, fmt, args);
    sb->length += needed;
    va_end(args);
}