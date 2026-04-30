#include "compiler.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// -- HELPER FUNCTIONS -- //
ResultType(LLVMTypeRef, charptr) getType(CometCompiler* compiler, char* typeName) {
    for (size_t i = 0; i < compiler->typeMapSize; i++) {
        if (strcmp(compiler->typeMap[i].typeName, typeName) == 0) {
            return Success(LLVMTypeRef, charptr, compiler->typeMap[i].llvmType);
        }
    }

    return Error(LLVMTypeRef, charptr, "The type was not found!");
}

// -- STATEMENTS -- //
ResultType(Nothing, charptr) visitProgram(CometCompiler* compiler, CometASTNode* node) {
    for (size_t i = 0; i < node->data.AST_PROGRAM.numStatements; i++) {
        ResultType(Nothing, charptr) result = compile(compiler, node->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;
    }

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) visitFuncDefStatement(CometCompiler* compiler, CometASTNode* node) {
    struct AST_FUNC_DEF_STATEMENT funcDef = node->data.AST_FUNC_DEF_STATEMENT;

    printf("%s\n", funcDef.returnType->data.AST_TYPE_NAME.name);
    ResultType(LLVMTypeRef, charptr) returnType = getType(compiler, funcDef.returnType->data.AST_TYPE_NAME.name);
    if (returnType.error)
        return Error(Nothing, charptr, returnType.as.error);

    // create function
    char* funcName = funcDef.ident->data.AST_IDENTIFIER.ident;
    LLVMTypeRef funcType = LLVMFunctionType(returnType.as.success, NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(compiler->module,funcName , funcType);

    // create function entry
    Estr entryName = CREATE_ESTR(funcName);
    APPEND_ESTR(entryName, "_entry");

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(compiler->context, function, entryName.str);
    LLVMPositionBuilderAtEnd(compiler->builder, entry);

    return Success(Nothing, charptr, {});
}
 
// -- MAIN --//
ResultType(Nothing, charptr) compileAST(CometCompiler* compiler, CometASTNode* root, const char* outputName) {
    ResultType(Nothing, charptr) compilerResult = compile(compiler, root);
    if (compilerResult.error)
        return compilerResult;

    LLVMPrintModuleToFile(compiler->module, outputName, NULL);

    return Success(Nothing, charptr, {});
}

ResultType(cometCompilerPtr, charptr) createCompiler(CometParser* parser) {
    CometCompiler* newCompiler = malloc(sizeof(CometCompiler));
    if (!newCompiler)
        return Error(cometCompilerPtr, charptr, "createCompiler: failed to allocate memory for compiler struct!");

    newCompiler->context = LLVMContextCreate();
    newCompiler->module = LLVMModuleCreateWithNameInContext("main", newCompiler->context);
    newCompiler->builder = LLVMCreateBuilderInContext(newCompiler->context);

    // create type map
    newCompiler->typeMapSize = 7;
    newCompiler->typeMap = calloc(newCompiler->typeMapSize, sizeof(CometLLVMTypePair));
    if (!newCompiler->typeMap)
        return Error(cometCompilerPtr, charptr, "createCompiler: failed to allocate memory for compiler type map!");

    LLVMTypeRef types[] = {
        LLVMIntTypeInContext(newCompiler->context, 8),   // small
        LLVMIntTypeInContext(newCompiler->context, 32),  // int
        LLVMIntTypeInContext(newCompiler->context, 64),  // big

        LLVMFloatTypeInContext(newCompiler->context),            // float
        LLVMDoubleTypeInContext(newCompiler->context),           // double

        LLVMIntTypeInContext(newCompiler->context, 8),  // bool

        LLVMVoidTypeInContext(newCompiler->context)             // void
    };
    
    for (size_t i = 0; i < newCompiler->typeMapSize; i++) {
        newCompiler->typeMap[i].typeName = (char*)BUILT_IN_TYPES[i];
        newCompiler->typeMap[i].llvmType = types[i];
    }

    return Success(cometCompilerPtr, charptr, newCompiler);
}

ResultType(Nothing, charptr) compile(CometCompiler* compiler, CometASTNode* node) {

    switch (node->nodeType) {
        case AST_PROGRAM:
            return visitProgram(compiler, node);

        case AST_FUNC_DEF_STATEMENT:
            return visitFuncDefStatement(compiler, node);

        default: {
            char* buffer = malloc(128);
            sprintf(buffer, "No visit method for %s node.", ASTNodeTypeToCStr(node->nodeType));
            return Error(Nothing, charptr, buffer);
        }
    }

    return Success(Nothing, charptr, {});
}