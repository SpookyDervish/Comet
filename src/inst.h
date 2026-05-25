#ifndef INST_H
#define INST_H

#include "../include/error.h"
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
    CO_REG,
    CO_IMMEDIATE
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
        uint32_t reg;
        CometImmediate imm;
    };
} CometOperand;

typedef enum {
    INST_MOV,
    INST_ADD
} CometInstType;

typedef struct {
    CometInstType opcode;
    CometOperand dest;
    CometOperand a;
    CometOperand b;
} CometInst;

#define NO_OPERAND ((CometOperand){0})

typedef struct {
    CometValueTypeKind inferredType;
    bool alive;
} CometTemp;

typedef struct {
    size_t programIdx;
    size_t regIdx;
    CometTemp temps[256];
    CometInst outputProgram[256];
} CometCompiler;

typedef CometCompiler* cometCompilerPtr;

Result(CometOperand, charptr);
Result(cometCompilerPtr, charptr);

ResultType(cometCompilerPtr, charptr) newCompiler();

char* cometImmediateToCStr(CometImmediate immediate);
char* cometOperandToCStr(CometOperand operand);
char* cometInstructionToCStr(CometInst inst);
char* cometInstOpcodeToCStr(CometInstType instType);

void buildInst(
    CometCompiler* c,
    CometInstType opcode,
    CometOperand dest,
    CometOperand a,
    CometOperand b
);
CometOperand buildAdd(CometCompiler* c, CometOperand a, CometOperand b);

#endif