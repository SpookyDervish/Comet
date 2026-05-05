#ifndef COMPILER_H
#define COMPILER_H

#include "../include/error.h"
#include "ast.h"
#include "struct.h"
#include "environment.h"
#include "parser.h"
#include <llvm-c/Types.h>
#include <stddef.h>
#include <llvm-c/Core.h>

extern const char* BUILT_IN_TYPES[];

typedef struct {
    char* typeName;
    LLVMTypeRef llvmType;
} CometLLVMTypePair;

typedef struct {
    LLVMValueRef value;
    LLVMTypeRef type;
    LLVMValueRef pointer;
    bool isPointer;
} CometValue;

typedef struct {
    LLVMValueRef a;
    LLVMValueRef b;
} LLVMValuePair;

UseList(LLVMTypeRef);
UseList(LLVMValueRef);
UseList(CometLLVMTypePair);

typedef struct {
    CometEnvironment* env;
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    List(CometLLVMTypePair) typeMap;
    List(StructInfo) structs;

    LLVMValueRef currentFunction;
} CometCompiler;
typedef CometCompiler* cometCompilerPtr;

Result(CometValue, charptr);
Result(LLVMTypeRef, charptr);
Result(LLVMValueRef, charptr);
Result(cometCompilerPtr, charptr);

ResultType(cometCompilerPtr, charptr) createCompiler(CometParser* parser);
ResultType(Nothing, charptr) compileAST(CometCompiler* compiler, CometASTNode* root, const char* outputName, bool outputLLVMIr, bool outputASM);
ResultType(int, charptr) compile(CometCompiler* compiler, CometASTNode* node);

#endif