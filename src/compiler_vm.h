#ifndef VM_H
#define VM_H

#include "ast.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    CVT_VOID,
    CVT_SMALL,
    CVT_INT,
    CVT_BIG,
    CVT_FLOAT,
    CVT_DOUBLE,
    CVT_BOOL
} CometValueType;

typedef struct {
    CometValueType type;
    union {
        char smallValue;
        int intValue;
        long long bigValue;
        float floatValue;
        double doubleValue;
        bool boolValue;
    };
} CometValue;

/*
Not the actual VM that runs the code.

This struct is used for register allocation and generation of instructions.
*/
typedef struct {
    CometValue registers[256];
    CometASTNode* ast;
} CometCVM;

#endif