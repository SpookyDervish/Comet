#ifndef COMPILER_STRUCT_H
#define COMPILER_STRUCT_H

#include "../include/list.h"
#include "../include/comet_operand.h"
#include "../include/struct.h"
#include "../include/serialized.h"
#include "typemap.h"
#include "generic.h"
#include <stdint.h>



typedef char* charptr;

UseList(CometInst);
typedef struct Block Block;
struct Block {
    List(CometInst) instructions;
    Block* parent;
};


UseList(uint64_t);
UseList(Block);

typedef struct {
    uint32_t stackIdx;
    uint32_t constIdx;
    uint32_t functionCount;
    uint32_t labelCount;

    bool includeDebugSymbols;
    uint64_t currentLine;
    List(uint64_t) debugInstInfo;

    char* inputFilePath;
    char* sourceCode;

    List(CachedGenericStruct) cachedGenerics;
    List(GenericStructDef) genericDefinitions;

    List(Block) blocks;
    Block* currentBlock;

    CometOperand consts[512];
    CometLabel* labels[512];
    CometFunction* functions[128];
    CometFunction* currentFunction;
    CometEnvironment* env;
    CometTypeMap* typeMap;
    List(cometStructPtr) structs;
    List(charptr) libs;
} CometCompiler;

typedef CometCompiler* cometCompilerPtr;

#endif