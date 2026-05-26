#ifndef SERIALIZED_H
#define SERIALIZED_H

#include <stdint.h>



typedef struct {
    char name[32];
    uint32_t startIdx;
} CometSerializedFunc;

typedef struct {
    uint8_t opcode;
    uint32_t a;
    uint32_t b;
    uint32_t c;
} CometSerializedInst;

#endif