#ifndef COMPILER_H
#define COMPILER_H

#include "../include/error.h"
#include "ast.h"
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
} CometTypeValuePair;

typedef struct {
    LLVMValueRef a;
    LLVMValueRef b;
} LLVMValuePair;

typedef struct {
    CometEnvironment* env;
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    CometLLVMTypePair* typeMap;
    size_t typeMapSize;

    LLVMValueRef currentFunction;
} CometCompiler;
typedef CometCompiler* cometCompilerPtr;

Result(CometTypeValuePair, charptr);
Result(LLVMTypeRef, charptr);
Result(LLVMValueRef, charptr);
Result(cometCompilerPtr, charptr);

ResultType(cometCompilerPtr, charptr) createCompiler(CometParser* parser);
ResultType(Nothing, charptr) compileAST(CometCompiler* compiler, CometASTNode* root, const char* outputName);
ResultType(Nothing, charptr) compile(CometCompiler* compiler, CometASTNode* node);

UseList(LLVMTypeRef);
UseList(LLVMValueRef);

#endif