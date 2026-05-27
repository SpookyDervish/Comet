#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "inst.h"
#include "../include/operand.h"
#include <stdint.h>

typedef struct {
    char name[32];
    uint32_t startIdx;
    uint32_t numArgs;
} CometSerializedFunc;

typedef struct {
    uint8_t opcode;
    int32_t a;
    int32_t b;
    int32_t c;
} CometSerializedInst;

CometSerializedInst* serializeInst(CometCompiler* c, CometInst inst);
uint32_t serializeOperand(CometCompiler* c, CometOperand operand);

#endif