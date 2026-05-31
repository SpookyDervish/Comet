#ifndef OPERAND_H
#define OPERAND_H

#include <stdint.h>
#include <stdbool.h>
#include "struct.h"

typedef struct {
    char name[32];
    uint32_t startIdx;
    uint32_t argCount;
} CometFunction;

typedef enum {
    COMET_VOID,
    COMET_SMALL,
    COMET_INT,
    COMET_BIG,
    COMET_FLOAT,
    COMET_DOUBLE,
    COMET_BOOL,
    COMET_STRUCT,
    COMET_FUNCTION
} CometValueTypeKind;

typedef struct {
    uint32_t pos;
    bool resolved;
} CometLabel;

typedef enum {
    CO_NONE,
    CO_IMMEDIATE,
    CO_STACK,
    CO_SYMBOL,
    CO_LABEL
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
        CometObject* objectVal;
    };
} CometImmediate;

typedef struct {
    CometValueTypeKind typeKind;
    union {
        CometStruct* structType;
        CometFunction* functionType;
    };
} CometType;

typedef struct {
    CometOperandKind type;
    union {
        uint32_t stackIdx;
        CometImmediate imm;
        uint32_t symbolIdx;
        CometLabel* label;
    };
} CometOperand;

#endif