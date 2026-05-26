#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "inst.h"
#include "operand.h"
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

CometSerializedInst* serializeInst(CometCompiler* c, CometInst inst);
uint32_t serializeOperand(CometCompiler* c, CometOperand operand);

#endif