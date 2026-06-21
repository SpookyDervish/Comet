#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

typedef struct {
    char fileName[32];
    char* sourceCode;
    uint64_t* instructions;
} DebugInfo;

#endif