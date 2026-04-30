#include "compiler.h"
#include "ast.h"
#include "environment.h"
#include "lexer.h"
#include "parser.h"
#include "token.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <llvm-c/Analysis.h>
#include <stddef.h>
#include <stdio.h>
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

ResultType(CometTypeValuePair, charptr) visitInfixExpression(CometCompiler* compiler, CometASTNode* node);
ResultType(CometTypeValuePair, charptr) resolveValue(CometCompiler* compiler, CometASTNode* node) {
    CometTypeValuePair res;

    switch (node->nodeType) {
        case AST_INT: {
            ResultType(LLVMTypeRef, charptr) intType = getType(compiler, "int");
            if (intType.error)
                return Error(CometTypeValuePair, charptr, intType.as.error);

            res = (CometTypeValuePair){
                LLVMConstInt(intType.as.success, node->data.AST_INT.number, false),
                intType.as.success
            };
            break;
        }

        case AST_DOUBLE: {
            ResultType(LLVMTypeRef, charptr) doubleType = getType(compiler, "double");
            if (doubleType.error)
                return Error(CometTypeValuePair, charptr, doubleType.as.error);

            res = (CometTypeValuePair){
                LLVMConstReal(doubleType.as.success, node->data.AST_DOUBLE.number),
                doubleType.as.success
            };
            break;
        }

        case AST_INFIX_EXPRESSION: {
            return visitInfixExpression(compiler, node);
        }

        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(compiler->env, varName);
            if (!varRecord) {
                Estr errMsg = CREATE_ESTR("Undefined variable \"");
                APPEND_ESTR(errMsg, varName);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometTypeValuePair, charptr, errMsg.str);
            }

            CometTypeValuePair result = {
                .value = LLVMBuildLoad2(compiler->builder, varRecord->type, varRecord->ptr, varName),
                .type = varRecord->type
            };
            return Success(CometTypeValuePair, charptr, result);

            break;
        }

        default:
            return Error(CometTypeValuePair, charptr, "Unkown expression type.");
    }

    return Success(CometTypeValuePair, charptr, res);
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

    for (size_t i = 0; i < funcDef.program->data.AST_PROGRAM.numStatements; i++) {
        ResultType(Nothing, charptr) result = compile(compiler, funcDef.program->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;
    }

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) visitReturnStatement(CometCompiler* compiler, CometASTNode* node) {
    
    ResultType(CometTypeValuePair, charptr) returnValue = resolveValue(compiler, node->data.AST_RETURN_STATEMENT.expression);
    
    if (returnValue.error)
        return Error(Nothing, charptr, returnValue.as.error);
    
    LLVMBuildRet(compiler->builder, returnValue.as.success.value);

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) visitExpressionStatement(CometCompiler* compiler, CometASTNode* node) {
    return compile(compiler, node->data.AST_EXPRESSION_STATEMENT.expression);
}

ResultType(Nothing, charptr) visitAssignStatement(CometCompiler* compiler, CometASTNode* node) {
    char* name = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;
    CometASTNode* value = node->data.AST_ASSIGN_STATEMENT.expression;
    char* type = node->data.AST_ASSIGN_STATEMENT.type->data.AST_TYPE_NAME.name;

    Record* varRecord = lookup(compiler->env, name);
    if (varRecord) {
        Estr errMsg = CREATE_ESTR("Redeclaration of variable \"");
        APPEND_ESTR(errMsg, name);
        APPEND_ESTR(errMsg, "\"");
        return Error(Nothing, charptr, errMsg.str);
    }

    ResultType(CometTypeValuePair, charptr) typeValuePair = resolveValue(compiler, value);
    if (typeValuePair.error)
        return Error(Nothing, charptr, typeValuePair.as.error);

    ResultType(LLVMTypeRef, charptr) varAssignType = getType(compiler, type);
    if (varAssignType.error)
        return Error(Nothing, charptr, varAssignType.as.error);

    if (varAssignType.as.success != typeValuePair.as.success.type) {
        Estr errMsg = CREATE_ESTR("Type mismatch when defining variable \"");
        APPEND_ESTR(errMsg, name);
        APPEND_ESTR(errMsg, "\"");
        return Error(Nothing, charptr, errMsg.str);
    }

    if (!lookup(compiler->env, name)) {
        LLVMValueRef ptr = LLVMBuildAlloca(compiler->builder, varAssignType.as.success, name);
        LLVMBuildStore(compiler->builder, typeValuePair.as.success.value, ptr);
        defineVar(compiler->env, name, ptr, varAssignType.as.success);
    }

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) visitReassignStatement(CometCompiler* compiler, CometASTNode* node) {
    char* name = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;
    CometASTNode* value = node->data.AST_ASSIGN_STATEMENT.expression;

    Record* varRecord = lookup(compiler->env, name);
    if (!varRecord) {
        Estr errMsg = CREATE_ESTR("Undefined variable \"");
        APPEND_ESTR(errMsg, name);
        APPEND_ESTR(errMsg, "\"");
        return Error(Nothing, charptr, errMsg.str);
    }

    ResultType(CometTypeValuePair, charptr) typeValuePair = resolveValue(compiler, value);
    if (typeValuePair.error)
        return Error(Nothing, charptr, typeValuePair.as.error);

    if (typeValuePair.as.success.type != varRecord->type) {
        Estr errMsg = CREATE_ESTR("You can't change the type of variable \"");
        APPEND_ESTR(errMsg, name);
        APPEND_ESTR(errMsg, "\" at runtime.");
        return Error(Nothing, charptr, errMsg.str);
    }

    LLVMBuildStore(compiler->builder, typeValuePair.as.success.value, varRecord->ptr);

    return Success(Nothing, charptr, {});
}


// -- EXPRESSIONS -- //
ResultType(CometTypeValuePair, charptr) visitInfixExpression(CometCompiler* compiler, CometASTNode* node) {
    CometToken op = node->data.AST_INFIX_EXPRESSION.op;

    ResultType(CometTypeValuePair, charptr) left = resolveValue(compiler, node->data.AST_INFIX_EXPRESSION.left);
    if (left.error) return Error(CometTypeValuePair, charptr, left.as.error);
    ResultType(CometTypeValuePair, charptr) right = resolveValue(compiler, node->data.AST_INFIX_EXPRESSION.right);
    if (right.error) return Error(CometTypeValuePair, charptr, right.as.error);

    // performing an operation on two ints
    ResultType(LLVMTypeRef, charptr) type;
    LLVMValueRef value;
    if (LLVMGetTypeKind(left.as.success.type) == LLVMIntegerTypeKind && LLVMGetTypeKind(right.as.success.type) == LLVMIntegerTypeKind) {
        type = getType(compiler, "int");

        switch (op.type) {
            case CT_PLUS: {
                // we pass NULL to let LLVM decide the name of the SSA output
                value = LLVMBuildAdd(compiler->builder, left.as.success.value, right.as.success.value, "tmp");
                break;
            }
            case CT_MINUS: {
                value = LLVMBuildSub(compiler->builder, left.as.success.value, right.as.success.value, "tmp");
                break;
            }
            case CT_TIMES: {
                value = LLVMBuildMul(compiler->builder, left.as.success.value, right.as.success.value, "tmp");
                break;
            }
            case CT_DIVIDE: {
                value = LLVMBuildSDiv(compiler->builder, left.as.success.value, right.as.success.value, "tmp");
                break;
            }

            default:
                return Error(CometTypeValuePair, charptr, "Unexpected operator for int and int!");
        }
    } else {
        return Error(CometTypeValuePair, charptr, "Cannot perform operation on those types.");
    }

    if (type.error)
        return Error(CometTypeValuePair, charptr, type.as.error);

    CometTypeValuePair result = {
        value, type.as.success
    };
    return Success(CometTypeValuePair, charptr, result);
}
 
// -- MAIN --//
ResultType(Nothing, charptr) compileAST(CometCompiler* compiler, CometASTNode* root, const char* outputName) {
    ResultType(Nothing, charptr) compilerResult = compile(compiler, root);
    if (compilerResult.error)
        return compilerResult;

    LLVMDumpModule(compiler->module);

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

    // set up env
    newCompiler->env = newEnvironment("root", NULL);

    return Success(cometCompilerPtr, charptr, newCompiler);
}

ResultType(Nothing, charptr) compile(CometCompiler* compiler, CometASTNode* node) {
    switch (node->nodeType) {
        case AST_PROGRAM:
            return visitProgram(compiler, node);

        case AST_FUNC_DEF_STATEMENT:
            return visitFuncDefStatement(compiler, node);
        case AST_RETURN_STATEMENT:
            return visitReturnStatement(compiler, node);
        case AST_EXPRESSION_STATEMENT:
            return visitExpressionStatement(compiler, node);
        case AST_ASSIGN_STATEMENT:
            return visitAssignStatement(compiler, node);
        case AST_REASSIGN_STATEMENT:
            return visitReassignStatement(compiler, node);

        case AST_INFIX_EXPRESSION: {
            ResultType(CometTypeValuePair, charptr) result = visitInfixExpression(compiler, node);
            if (result.error)
                return Error(Nothing, charptr, result.as.error);

            return Success(Nothing, charptr, {});
        }

        default: {
            char* buffer = malloc(128);
            sprintf(buffer, "No visit method for %s node.", ASTNodeTypeToCStr(node->nodeType));
            return Error(Nothing, charptr, buffer);
        }
    }

    return Success(Nothing, charptr, {});
}