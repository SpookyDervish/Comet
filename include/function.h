#ifndef FUNCTION_H
#define FUNCTION_H

#include <stdio.h>
#include "type.h"

struct CometFunction {
    char name[32];
    uint32_t startIdx;
    uint32_t argCount;
    CometType returnType;
    bool isMethod;
    bool isExternal;
};

#endif