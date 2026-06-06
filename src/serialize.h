#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "inst.h"
#include "../include/comet_operand.h"
#include <stdint.h>

typedef struct {
    char name[32];
    uint64_t startIdx;
    uint32_t numArgs;
} CometSerializedFunc;

typedef struct {
    uint8_t opcode;
    int32_t a;
    int32_t b;
    int32_t c;
} CometSerializedInst;

typedef struct {
    uint32_t numFields;
    uint32_t numMethods;
    uint32_t* vtable;
} CometSerializedStruct;

typedef struct {
    uint32_t numFields;
    uint32_t numMethods;
} CometSerializedStructHeader;

CometSerializedInst* serializeInst(CometInst inst);
CometSerializedStruct* serializeStruct(CometStruct* structType);
uint32_t serializeOperand(CometOperand operand);

#endif