#ifndef INST_H
#define INST_H

#include "../include/error.h"
#include "environment.h"
#include "../include/operand.h"
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
    INST_CALL,
    INST_EQ,
    INST_JMP,
    INST_JMP_IF_FALSE,
    INST_NOT
} CometInstType;

typedef struct {
    CometInstType opcode;
    CometOperand a;
    CometOperand b;
    CometOperand c;
    uint32_t pos;
} CometInst;

#define NO_OPERAND ((CometOperand){0})

typedef struct {
    uint32_t programIdx;
    uint32_t stackIdx;
    uint32_t constIdx;
    uint32_t functionCount;
    uint32_t labelCount;
    CometOperand consts[512];
    CometLabel* labels[512];
    CometInst outputProgram[2048];
    CometFunction* functions[128];
    CometEnvironment* env;
} CometCompiler;

typedef CometCompiler* cometCompilerPtr;

Result(CometOperand, charptr);
Result(cometCompilerPtr, charptr);

ResultType(cometCompilerPtr, charptr) newCompiler();

char* cometImmediateToCStr(CometImmediate immediate);
char* cometOperandToCStr(CometCompiler* c ,CometOperand operand);
char* cometInstructionToCStr(CometCompiler* c, CometInst inst);
char* cometInstOpcodeToCStr(CometInstType instType);

CometOperand pushVal(CometCompiler* c);
void popVal(CometCompiler* c);

UseList(CometOperand);

CometFunction* getSymbol(CometCompiler* c, CometOperand symbolValue);

uint32_t getSymbolIndex(CometCompiler* c, const char* symbolName);
CometOperand findConst(CometCompiler* c, CometOperand value);

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
CometOperand buildEq(CometCompiler* c);
CometOperand buildFunction(CometCompiler* c, char* name, uint32_t argCount);
void buildReturn(CometCompiler* c);
CometOperand buildLoadArg(CometCompiler* c, uint32_t idx);
CometOperand buildCall(CometCompiler* c, char* name, List(CometOperand) args);
void buildJump(CometCompiler* c, CometLabel* label);
void buildJumpIfFalse(CometCompiler* c, CometLabel* label);
CometOperand buildNot(CometCompiler* c);

CometLabel* buildLabel(CometCompiler* c);
void resolveLabel(CometCompiler* c, CometLabel* label);

#endif