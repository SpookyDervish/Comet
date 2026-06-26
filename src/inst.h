#ifndef INST_H
#define INST_H

#include "../include/error.h"
#include "../include/list.h"
#include "../include/comet_operand.h"
#include "../include/struct.h"
#include "../include/serialized.h"
#include "../include/function.h"
#include "../include/debug.h"
#include "../include/error_message.h"
#include "ast.h"
#include "typemap.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define NO_OPERAND ((CometOperand){0})

typedef CometStruct* cometStructPtr;

typedef struct {
    char* name;
    CometASTNode* structDefNode;
} GenericStructDef;
UseList(GenericStructDef);

UseList(CometType);
typedef struct {
    CometStruct* structType; // the result of the generic. e.g: Box_int
    List(CometType) genericTypes; // the types of all the generics. e.g: genericTypes[0] = T, T = int
    char* baseStructName; // the name of the base generic. e.g: Box
    GenericStructDef structDef; // ast node of struct def
} CachedGenericStruct;

UseList(cometStructPtr);
UseList(charptr);
UseList(uint64_t);
UseList(CachedGenericStruct);

typedef struct {
    uint32_t programIdx;
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

    CometOperand consts[512];
    CometLabel* labels[512];
    CometInst* outputProgram;
    CometFunction* functions[128];
    CometFunction* currentFunction;
    CometEnvironment* env;
    CometTypeMap* typeMap;
    List(cometStructPtr) structs;
    List(charptr) libs;
} CometCompiler;

typedef CometCompiler* cometCompilerPtr;

Result(CometOperand, ErrorMessage);

bool typesAreEqual(CometType a, CometType b);


CometType getValueType(CometCompiler* c, CometOperand value);

CometOperand pushVal(CometCompiler* c);
void popVal(CometCompiler* c);

CometFunction* getSymbol(CometCompiler* c, CometOperand symbolValue);

int32_t getSymbolIndex(CometCompiler* c, const char* symbolName);
CometOperand findConst(CometCompiler* c, CometOperand value);

bool typeIsInt(CometType type);
bool typeIsFloat(CometType type);

void buildInst(
    CometCompiler* c,
    CometInstType opcode,
    CometOperand dest,
    CometOperand a,
    CometOperand b
);
CometOperand storeConst(CometCompiler* c, CometOperand value);
void buildPushConst(CometCompiler* c, CometOperand idx);
void buildStore(CometCompiler* c, uint32_t idx);
CometOperand buildLoad(CometCompiler* c, uint32_t idx);
CometOperand buildAdd(CometCompiler* c, CometType resultType);
CometOperand buildSub(CometCompiler* c, CometType resultType);
CometOperand buildMul(CometCompiler* c, CometType resultType);
CometOperand buildDiv(CometCompiler* c, CometType resultType);
CometOperand buildEq(CometCompiler* c, CometType resultType);
CometOperand buildNeq(CometCompiler* c, CometType resultType);
CometOperand buildLt(CometCompiler* c, CometType resultType);
CometOperand buildGt(CometCompiler* c, CometType resultType);
CometOperand buildLte(CometCompiler* c, CometType resultType);
CometOperand buildGte(CometCompiler* c, CometType resultType);
void buildReturn(CometCompiler* c);
CometOperand buildLoadArg(CometCompiler* c, uint32_t idx);
CometOperand buildCall(CometCompiler* c, char* name, List(CometOperand) args);
void buildJump(CometCompiler* c, CometLabel* label);
void buildJumpIfFalse(CometCompiler* c, CometLabel* label);
void buildJumpIfTrue(CometCompiler* c, CometLabel* label);
CometOperand buildNot(CometCompiler* c);
CometOperand buildI2F(CometCompiler* c);
CometOperand buildF2I(CometCompiler* c);
void buildDup(CometCompiler* c);
CometOperand buildNew(CometCompiler* c, uint32_t idx);
CometOperand buildBuildList(CometCompiler* c);
CometOperand buildListAt(CometCompiler* c);
void buildListSet(CometCompiler* c);
CometOperand buildGetField(CometCompiler* c, uint32_t idx);
void buildSetField(CometCompiler* c, uint32_t idx);
CometOperand buildCallMethod(CometCompiler* c, uint32_t vtableIdx, List(CometOperand) args);
void buildBreakpoint(CometCompiler* c);
void buildTry(CometCompiler* c);
void buildEndTry(CometCompiler* c);
void buildThrow(CometCompiler* c);
CometOperand buildListLength(CometCompiler* c);
CometOperand buildGetExcept(CometCompiler* c);
CometType buildCast(CometCompiler* c, CometType before, CometType after);

CometOperand buildFunction(CometCompiler* c, char* name, uint32_t argCount, CometType returnType, CometType* argTypes, bool isVarArgs, bool isMethod, bool isExternal, int8_t libIdx);

CometLabel* buildLabel(CometCompiler* c);
void resolveLabel(CometCompiler* c, CometLabel* label);

#endif