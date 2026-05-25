#ifndef OPERAND_H
#define OPERAND_H

#include <stdint.h>

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

#endif