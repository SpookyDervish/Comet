#ifndef COMPILER_H
#define COMPILER_H

#include "../include/error.h"
#include "ast.h"
#include "environment.h"
#include "parser.h"
#include <stddef.h>
#include <tram.h>

UseList(Tram_Register);

// used for register liveness
typedef struct Block {
    List(astNodePtr) statements;

    struct Block** successors;
    int succCount;

    CometEnvironment* env;

    uint32_t use; // this is a bitmask with 32 entries (16 GP reg, 16 float reg)
    uint32_t def;
    uint32_t liveIn;
    uint32_t liveOut;
} Block;
typedef Block* blockPtr;

UseList(blockPtr);
typedef struct {
    List(blockPtr) blocks;

    Block* entry;
    Block* exit;
} FunctionCFG;
typedef FunctionCFG* funcCfgPtr;

typedef struct {
    FunctionCFG* cfg;
    Block* current;
} CFGBuilder;

typedef struct {
    int useCount[Tram_Register_f15+1];
} Liveness;

typedef struct {
    Tram_Parameter val;
    char* type;
} ValStructPair;

typedef struct {
    Tram_Program* program;
    Tram_Register* usedRegisters;
    Tram_Register* usedFloatRegisters;
    CometEnvironment* env;
    Liveness liveness;
    FunctionCFG* currentFunc;
} CometCompiler;

Result(blockPtr, charptr);
Result(CometCompiler, charptr);
Result(Tram_Register, charptr);
Result(ValStructPair, charptr);
Result(funcCfgPtr, charptr);

ResultType(CometCompiler, charptr) createCompiler(CometParser* parser);
ResultType(Nothing, charptr) compileAST(CometCompiler* compiler, CometASTNode* root, char* outputName);
ResultType(Nothing, charptr) compile(CometCompiler* compiler, CometASTNode* node);

#endif