#ifndef FUNCTION_H
#define FUNCTION_H

#include <stdio.h>
#include "type.h"

typedef struct CometFunction CometFunction;
struct CometFunction {
    char name[32];
    uint32_t startIdx;
    uint32_t argCount;
    CometType returnType;
    CometType* argTypes;
    bool isVarArgs;
    bool isMethod;
    bool isExternal;
    int8_t libIdx;
};

#endif