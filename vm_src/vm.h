#ifndef VM_H
#define VM_H

#define MAX_CALL_FRAMES 1024
#define MAX_DATA_STACK 65536
#define MAX_VARIABLES 1024

#include "../include/operand.h"
#include "../lib/list.h"
#include "args.h"
#include "serialized.h"
#include <stdint.h>

typedef struct DebuggerBreakpoint DebuggerBreakpoint;
UseList(DebuggerBreakpoint);

typedef struct {
    uint64_t ip;
    uint32_t stackStart;
    int64_t args[128];
    char* funcName;
} Frame;

typedef struct {
    uint32_t numConstants;
    CometOperand* constants;

    int64_t stack[MAX_DATA_STACK];

    uint32_t numFunctions;

    uint64_t numInstructions;
    CometSerializedInst* instructions;

    CometSerializedFunc* functions;

    uint32_t numStructs;
    CometSerializedStruct* structs;

    Frame callStack[MAX_CALL_FRAMES];
    Frame* currentFrame;
    uint32_t callIdx;

    uint32_t sp;

    int64_t variables[MAX_VARIABLES];

    uint8_t* breakpoints;
    uint8_t nextBreakpointID;
    uint64_t instructionsLeftToExec;

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