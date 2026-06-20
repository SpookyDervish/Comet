#ifndef VM_H
#define VM_H

#define MAX_CALL_FRAMES 1024
#define MAX_DATA_STACK 65536
#define MAX_VARIABLES 1024
#define MAX_EXCEPTIONS 16

#include "comet_operand.h"
#include "list.h"
#include "error.h"
#include "serialized.h"
#include <stdint.h>

Result(int, charptr);

typedef struct DebuggerBreakpoint DebuggerBreakpoint;
UseList(DebuggerBreakpoint);

typedef struct {
    uint64_t ip;
    //uint32_t stackStart;
    int64_t args[128];
    char* funcName;
} Frame;

typedef struct CometVM CometVM;
typedef int64_t (*externalLibFunc)(int64_t args[], CometVM* vm);

typedef struct {
    uint64_t handlerIP;
    uint32_t restoredSP;
} ExceptFrame;

struct CometVM {
    uint32_t numConstants;
    CometOperand* constants;

    int64_t stack[MAX_DATA_STACK];

    void** loadedLibs;
    externalLibFunc* externalFuncs;

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

    ExceptFrame exceptStack[MAX_EXCEPTIONS];
    uint32_t currentExcept;

    bool running;
};

typedef CometVM* vmPtr;

Result(vmPtr, charptr);

ResultType(vmPtr, charptr) newCometVM(char* filePath);
ResultType(int, charptr) startVM(CometVM* vm);

// Functions exposed so external libs can use them. //
/*
Call a function. The callee must return the VM's state to how it was before
or suffer the consequences.
*/
void callFunction(CometVM* vm, CometSerializedFunc* function, uint8_t callArgs);

/*
Return from the function the VM is currently in.
*/
void returnFromFunc(CometVM* vm);

#endif