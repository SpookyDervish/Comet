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
    INST_MUL,
    INST_LOAD_ARG,
    INST_RET,
    INST_CALL
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
    uint32_t functionCount;
    CometOperand consts[512];
    CometInst outputProgram[2048];
    CometFunction* functions[128];
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

UseList(CometOperand);

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
CometOperand buildFunction(CometCompiler* c, char* name, uint32_t argCount);
void buildReturn(CometCompiler* c, CometOperand value);
CometOperand buildLoadArg(CometCompiler* c, uint32_t idx);
CometOperand buildCall(CometCompiler* c, char* name, List(CometOperand) args);

#endif