#ifndef VM_H
#define VM_H

#include "../include/operand.h"
#include "args.h"
#include "serialized.h"
#include <stdint.h>



typedef struct {
    CometOperand* constants;
    uint64_t stackCapacity;
    int64_t* stack;
    CometSerializedInst* instructions;
    CometSerializedFunc* functions;
} CometVM;

typedef CometVM* vmPtr;

Result(vmPtr, charptr);

ResultType(vmPtr, charptr) newCometVM(char* filePath);

#endif