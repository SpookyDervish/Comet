#ifndef VM_H
#define VM_H

#include "../include/operand.h"
#include "../lib/list.h"
#include "args.h"
#include "serialized.h"
#include <stdint.h>

typedef struct DebuggerBreakpoint DebuggerBreakpoint;
UseList(DebuggerBreakpoint);

typedef struct {
    int64_t* stack;
    int64_t* args;
    uint64_t ip;
    uint32_t sp;
    char* funcName;
} Frame;

typedef struct {
    uint32_t numConstants;
    CometOperand* constants;

    uint64_t stackCapacity;
    int64_t** currentStack;

    uint32_t numFunctions;

    uint64_t numInstructions;
    CometSerializedInst* instructions;

    CometSerializedFunc* functions;

    uint32_t numStructs;
    CometSerializedStruct* structs;

    Frame** callStack;
    Frame* currentFrame;
    uint8_t callIdx;

    uint32_t* currentSp;

    int64_t* variables;

    List(DebuggerBreakpoint) breakpoints;
    uint8_t nextBreakpointID;

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