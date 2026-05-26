#ifndef INST_H
#define INST_H

#include "../include/error.h"
#include "environment.h"
#include "operand.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>



typedef enum {
    INST_PUSH_CONST,
    INST_STORE,
    INST_LOAD,
    INST_ADD,
    INST_SUB,
    INST_MUL
} CometInstType;

typedef struct {
    CometInstType opcode;
    CometOperand a;
    CometOperand b;
    CometOperand c;
} CometInst;

#define NO_OPERAND ((CometOperand){0})

typedef struct {
    uint32_t programIdx;
    uint32_t stackIdx;
    uint32_t constIdx;
    CometOperand consts[256];
    CometInst outputProgram[256];
    CometEnvironment* env;
} CometCompiler;

typedef CometCompiler* cometCompilerPtr;

Result(CometOperand, charptr);
Result(cometCompilerPtr, charptr);

ResultType(cometCompilerPtr, charptr) newCompiler();

char* cometImmediateToCStr(CometImmediate immediate);
char* cometOperandToCStr(CometOperand operand);
char* cometInstructionToCStr(CometCompiler* c, CometInst inst);
char* cometInstOpcodeToCStr(CometInstType instType);

CometOperand pushVal(CometCompiler* c);
void popVal(CometCompiler* c);

void buildInst(
    CometCompiler* c,
    CometInstType opcode,
    CometOperand dest,
    CometOperand a,
    CometOperand b
);
CometOperand storeConst(CometCompiler* c, CometOperand value);
void buildPushConst(CometCompiler* c, CometOperand idx);
void buildStore(CometCompiler* c, uint32_t idx);
CometOperand buildLoad(CometCompiler* c, uint32_t idx);
CometOperand buildAdd(CometCompiler* c);
CometOperand buildSub(CometCompiler* c);
CometOperand buildMul(CometCompiler* c);

#endif