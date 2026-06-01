#ifndef INST_H
#define INST_H

#include "../lib/error.h"
#include "../lib/list.h"
#include "environment.h"
#include "../include/operand.h"
#include "../include/struct.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>



typedef enum {
    INST_PUSH_CONST,
    INST_STORE,
    INST_LOAD,
    INST_ADDI,
    INST_ADDF,
    INST_SUBI,
    INST_SUBF,
    INST_MULI,
    INST_MULF,
    INST_DIVI,
    INST_DIVF,
    INST_LOAD_ARG,
    INST_RET,
    INST_CALL,
    INST_EQI,
    INST_EQF,
    INST_NEQI,
    INST_NEQF,
    INST_GTI,
    INST_GTF,
    INST_LTI,
    INST_LTF,
    INST_GTEI,
    INST_GTEF,
    INST_LTEI,
    INST_LTEF,
    INST_JMP,
    INST_JMP_IF_FALSE,
    INST_NOT,
    INST_I2F,
    INST_DUP,
    INST_NEW,
    INST_GET_FIELD,
    INST_SET_FIELD,
    INST_CALL_METHOD
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
    char* name;
    CometType type;
} CometTypeMapEntry;

typedef CometStruct* cometStructPtr;

UseList(CometTypeMapEntry);
UseList(cometStructPtr);

typedef struct {
    uint32_t programIdx;
    uint32_t stackIdx;
    uint32_t constIdx;
    uint32_t functionCount;
    uint32_t labelCount;
    CometOperand consts[512];
    CometLabel* labels[512];
    CometInst* outputProgram;
    CometFunction* functions[128];
    CometEnvironment* env;
    List(CometTypeMapEntry) typeMap;
    List(cometStructPtr) structs;
} CometCompiler;

typedef CometCompiler* cometCompilerPtr;

Result(CometOperand, charptr);
Result(cometCompilerPtr, charptr);

ResultType(cometCompilerPtr, charptr) newCompiler();

bool typesAreEqual(CometType a, CometType b);

char* cometImmediateToCStr(CometImmediate immediate);
char* cometOperandToCStr(CometCompiler* c ,CometOperand operand);
char* cometInstructionToCStr(CometCompiler* c, CometInst inst);
char* cometInstOpcodeToCStr(CometInstType instType);

CometType getValueType(CometCompiler* c, CometOperand value);

CometOperand pushVal(CometCompiler* c);
void popVal(CometCompiler* c);

UseList(CometOperand);

CometFunction* getSymbol(CometCompiler* c, CometOperand symbolValue);

int32_t getSymbolIndex(CometCompiler* c, const char* symbolName);
CometOperand findConst(CometCompiler* c, CometOperand value);

bool typeIsInt(CometType type);

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
CometOperand buildAdd(CometCompiler* c, CometType resultType);
CometOperand buildSub(CometCompiler* c, CometType resultType);
CometOperand buildMul(CometCompiler* c, CometType resultType);
CometOperand buildDiv(CometCompiler* c, CometType resultType);
CometOperand buildEq(CometCompiler* c, CometType resultType);
CometOperand buildNeq(CometCompiler* c, CometType resultType);
CometOperand buildLt(CometCompiler* c, CometType resultType);
CometOperand buildGt(CometCompiler* c, CometType resultType);
CometOperand buildLte(CometCompiler* c, CometType resultType);
CometOperand buildGte(CometCompiler* c, CometType resultType);
CometOperand buildFunction(CometCompiler* c, char* name, uint32_t argCount, CometType returnType, bool isMethod);
void buildReturn(CometCompiler* c);
CometOperand buildLoadArg(CometCompiler* c, uint32_t idx);
CometOperand buildCall(CometCompiler* c, char* name, List(CometOperand) args);
void buildJump(CometCompiler* c, CometLabel* label);
void buildJumpIfFalse(CometCompiler* c, CometLabel* label);
CometOperand buildNot(CometCompiler* c);
CometOperand buildI2F(CometCompiler* c);
void buildDup(CometCompiler* c);
CometOperand buildNew(CometCompiler* c, uint32_t idx);
CometOperand buildGetField(CometCompiler* c, uint32_t idx);
void buildSetField(CometCompiler* c, uint32_t idx);
CometOperand buildCallMethod(CometCompiler* c, uint32_t vtableIdx, List(CometOperand) args);
CometType buildCast(CometCompiler* c, CometType before, CometType after);

CometLabel* buildLabel(CometCompiler* c);
void resolveLabel(CometCompiler* c, CometLabel* label);

#endif