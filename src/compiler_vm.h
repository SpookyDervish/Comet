#ifndef VM_H
#define VM_H

#include "ast.h"
#include "lexer.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../include/error.h"

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
    CO_CONST
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
} CometConstant;

typedef struct {
    CometOperandKind type;
    union {
        uint32_t reg;
        uint32_t constIdx;
    };
} CometOperand;

typedef enum {
    MOV,
    ADD
} CometInstType;

typedef struct {
    CometInstType opcode;
    CometOperand dest;
    CometOperand a;
    CometOperand b;
} CometInst;

typedef struct {
    CometValueTypeKind inferredType;
    bool alive;
} CometTemp;

/*
Not the actual VM that runs the code.

This struct is used for register allocation and generation of instructions.
*/
typedef struct {
    CometTemp temps[256];
    CometInst outputProgram[16];
    CometConstant consts[256];
} CometCompiler;

typedef void* voidPtr;

Result(voidPtr, charptr);
Result(CometOperand, charptr);

ResultType(voidPtr, charptr) compile(CometCompiler* c, CometASTNode* node);
CometOperand createOperand(CometOperandKind type);

#endif