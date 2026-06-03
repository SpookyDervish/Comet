#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "args.h"
#include "serialized.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "../lib/error.h"
#include "../lib/list.h"

#define IS_OBJECT(x) _Generic((x), \
    CometObject*: 1,        \
    default: 0              \
)

typedef struct {
    int hasEnd;
    uint32_t start;
    uint32_t end;
} Range;

typedef struct {
    CometVM* vm;
    uint8_t* breakpoints;
    bool running;
} CometDebugger;

Result(charptr, charptr);

typedef struct {
    const char* name;
    ResultType(charptr, charptr) (*handler)(CometDebugger* dbgr, int argc, char** argv);
    const char* help;
    const char* usage;
    const char** aliases;
} CometDebugCommand;

extern const CometDebugCommand DBGR_COMMANDS[];

char* cometImmediateToCStr(CometImmediate immediate);
char* cometOperandToCStr(CometVM* vm ,CometOperand operand);
char* cometInstructionToCStr(CometVM* vm, CometSerializedInst inst, uint64_t instPos);
char* cometInstOpcodeToCStr(CometInstType instType);

void startDebugger(CometVM* vm, bool startedFromStep);

char* stackTrace(CometVM* vm);
char* stackAsString(int64_t* stack, uint32_t sp);

Range parseRange(const char* str);

#endif