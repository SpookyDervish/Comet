#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "args.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "../lib/error.h"

typedef struct {
    CometVM* vm;
} CometDebugger;

typedef struct {
    const char* name;
    ResultType(voidPtr, charptr) (*handler)(CometDebugger* dbgr, int argc, char** argv);
    const char* help;
    const char* usage;
    const char** aliases;
} CometDebugCommand;

extern const CometDebugCommand DBGR_COMMANDS[];

void startDebugger(CometVM* vm);

char* stackTrace(CometVM* vm);
char* stackAsString(int64_t* stack, uint32_t sp);

#endif