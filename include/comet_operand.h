#ifndef OPERAND_H
#define OPERAND_H

#include <stdint.h>
#include <stdbool.h>
#include "struct.h"
#include "type.h"

typedef struct CometOperand CometOperand;

typedef enum {
    CO_NONE,
    CO_IMMEDIATE,
    CO_STACK,
    CO_SYMBOL,
    CO_LABEL
} CometOperandKind;

typedef struct {
    uint32_t length;
    uint32_t capacity;
    CometOperand* data;
} CometArray;

typedef struct CometEnvironment CometEnvironment;
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
        CometArray* arrayVal;
        CometEnvironment* moduleVal;
        CometType typeVal;
    };
} CometImmediate;

struct CometOperand {
    CometOperandKind type;
    union {
        uint32_t stackIdx;
        CometImmediate imm;
        uint32_t symbolIdx;
        CometLabel* label;
    };
};

UseList(CometOperand);

typedef struct {
    CometFunctionType funcType;
    CometOperand value;
    CometOperand methodIdx;
} CometFunctionTypeInfo;

#endif