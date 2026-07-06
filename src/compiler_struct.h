#ifndef COMPILER_STRUCT_H
#define COMPILER_STRUCT_H

#include "../include/list.h"
#include "../include/comet_operand.h"
#include "../include/struct.h"
#include "../include/serialized.h"
#include "../include/typemap.h"
#include "generic.h"
#include <stdint.h>



typedef char* charptr;

typedef CometLabel* labelPtr;

UseList(CometInst);
UseList(labelPtr);
typedef struct Block Block;
struct Block {
    List(CometInst) instructions;
    List(labelPtr) labels;
    Block* parent;
};

typedef struct {
    CometLabel* breakLabel;
    CometLabel* continueLabel;
} LoopContext;
UseList(LoopContext);


UseList(uint64_t);
UseList(Block);

typedef struct {
    uint32_t stackIdx;
    uint32_t constIdx;
    uint32_t functionCount;

    bool includeDebugSymbols;
    uint64_t currentLine;
    List(uint64_t) debugInstInfo;

    char* inputFilePath;
    char* sourceCode;

    List(CachedGenericStruct) cachedGenerics;
    List(GenericStructDef) genericDefinitions;

    List(Block) blocks;
    Block* currentBlock;
    List(LoopContext) loopContexts;

    CometOperand consts[512];
    CometFunction* functions[128];
    CometFunction* currentFunction;
    CometStruct* currentStruct;
    CometEnvironment* env;
    CometEnvironment* rootEnv;
    CometTypeMap* typeMap;
    List(cometStructPtr) structs;
    List(charptr) libs;
} CometCompiler;

typedef CometCompiler* cometCompilerPtr;

#endif