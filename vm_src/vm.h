#ifndef VM_H
#define VM_H

#include "../include/operand.h"
#include "args.h"
#include "serialized.h"
#include <stdint.h>



typedef struct {
    uint32_t numConstants;
    CometOperand* constants;

    uint64_t stackCapacity;
    int64_t* stack;

    uint32_t numFunctions;
    CometSerializedInst* instructions;

    CometSerializedFunc* functions;

    CometSerializedFunc* callStack[128];
    uint8_t callIdx;

    uint32_t ip;
    uint32_t sp;

    int64_t* variables;

    bool running;
} CometVM;

typedef CometVM* vmPtr;
typedef void* voidPtr;

Result(vmPtr, charptr);
Result(int, charptr);
Result(voidPtr, charptr);

ResultType(vmPtr, charptr) newCometVM(char* filePath);
ResultType(int, charptr) startVM(CometVM* vm);

#endif