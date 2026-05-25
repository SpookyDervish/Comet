#ifndef INST_H
#define INST_H

#include "../include/error.h"
#include "environment.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    COMET_VOID,
    COMET_SMALL,
    COMET_INT,
    COMET_BIG,
    COMET_FLOAT,
    COMET_DOUBLE,
    COMET_BOOL
} CometValueTypeKind;

typedef enum {
    CO_IMMEDIATE,
    CO_STACK
} CometOperandKind;

typedef struct {
    CometValueTypeKind typeKind;
    union {
        int8_t smallVal;
        int32_t intVal;
        int64_t bigVal;
        float floatVal;
        double doubleVal;
        bool boolVal;
    };
} CometImmediate;

typedef struct {
    CometOperandKind type;
    union {
        uint32_t stackIdx;
        CometImmediate imm;
    };
} CometOperand;

typedef enum {
    INST_PUSH_CONST,
    INST_ADD,
    INST_SUB,
    INST_MUL
} CometInstType;

typedef struct {
    CometInstType opcode;
    CometOperand dest;
    CometOperand a;
    CometOperand b;
} CometInst;

#define NO_OPERAND ((CometOperand){0})

typedef struct {
    uint32_t programIdx;
    uint32_t stackIdx;
    uint32_t constIdx;
    CometOperand consts[256];
    CometInst outputProgram[256];
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
void buildPushConst(CometCompiler* c, CometOperand idx);
CometOperand storeConst(CometCompiler* c, CometOperand value);
CometOperand buildAdd(CometCompiler* c);
CometOperand buildSub(CometCompiler* c);
CometOperand buildMul(CometCompiler* c);

#endif