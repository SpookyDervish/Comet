#include "compiler.h"
#include "ast.h"
#include "environment.h"
#include "lexer.h"
#include "parser.h"
#include "token.h"
#include "util.h"
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

LLVMValuePair verifyInts(CometCompiler* compiler, LLVMValueRef a, LLVMValueRef b) {
    LLVMTypeRef aType = LLVMTypeOf(a);
    LLVMTypeRef bType = LLVMTypeOf(b);

    unsigned aWidth = LLVMGetIntTypeWidth(aType);
    unsigned bWidth = LLVMGetIntTypeWidth(bType);

    if (aWidth < bWidth) {
        a = LLVMBuildSExt(compiler->builder, a, bType, "cast");
    } else {
        b = LLVMBuildSExt(compiler->builder, b, aType, "cast");
    }

    return (LLVMValuePair){a, b};
}

LLVMValueRef castInt(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef targetType,
    bool isSigned
) {
    LLVMTypeRef srcType = LLVMTypeOf(value);

    unsigned srcBits = LLVMGetIntTypeWidth(srcType);
    unsigned dstBits = LLVMGetIntTypeWidth(targetType);

    if (srcBits == dstBits) {
        return value;
    }

    if (srcBits < dstBits) {
        return isSigned
            ? LLVMBuildSExt(builder, value, targetType, "sext")
            : LLVMBuildZExt(builder, value, targetType, "zext");
    }

    // narrowing
    return LLVMBuildTrunc(builder, value, targetType, "trunc");
}

ResultType(LLVMValueRef, charptr) castToType(LLVMBuilderRef builder, LLVMValueRef value, LLVMTypeRef targetType) {
    LLVMTypeRef srcType = LLVMTypeOf(value);
    LLVMTypeKind srcTypeKind = LLVMGetTypeKind(srcType);

    LLVMTypeKind targetTypeKind = LLVMGetTypeKind(targetType);

    if (srcTypeKind == LLVMIntegerTypeKind) {
        if (targetTypeKind == LLVMIntegerTypeKind)
            return Success(LLVMValueRef, charptr, castInt(builder, value, targetType, true));
        else if (targetTypeKind == LLVMFloatTypeKind || targetTypeKind == LLVMDoubleTypeKind) 
            return Success(LLVMValueRef, charptr, LLVMBuildSIToFP(
                builder,
                value,
                targetType,
                "sitof"
            ));
    }

    return Error(LLVMValueRef, charptr, "Cannot cast those types.");
}

ResultType(CometTypeValuePair, charptr) convertString(CometCompiler* compiler, char* str) {
    // replace escape sequences
    char* newStr = repl_str(str, "\\n", "\n");

    LLVMValueRef strConst = LLVMConstStringInContext(compiler->context, newStr, strlen(newStr), false);

    LLVMValueRef global = LLVMAddGlobal(compiler->module, LLVMTypeOf(strConst), "str");
    LLVMSetInitializer(global, strConst);
    LLVMSetGlobalConstant(global, true);
    LLVMSetLinkage(global, LLVMPrivateLinkage);

    LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(compiler->context), 0, false);
    LLVMValueRef indices[] = { zero, zero };
    LLVMValueRef ptr = LLVMConstGEP2(
        LLVMTypeOf(strConst),
        global,
        indices,
        2
    );

    CometTypeValuePair res = {
        .value = ptr,
        .type = LLVMTypeOf(ptr)
    };
    return Success(CometTypeValuePair, charptr, res);
}

ResultType(CometTypeValuePair, charptr) visitInfixExpression(CometCompiler* compiler, CometASTNode* node);
ResultType(CometTypeValuePair, charptr) visitFuncCall(CometCompiler* compiler, CometASTNode* node);
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

        case AST_STRING: {
            ResultType(CometTypeValuePair, charptr) stringType = convertString(compiler, node->data.AST_STRING.value);
            if (stringType.error)
                return stringType;

            res = stringType.as.success;
            break;
        }

        case AST_INFIX_EXPRESSION: {
            return visitInfixExpression(compiler, node);
        }

        case AST_FUNC_CALL: {
            return visitFuncCall(compiler, node);
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

ResultType(Nothing, charptr) compileBlock(CometCompiler* compiler, CometASTNode* block) {
    for (size_t i = 0; i < block->data.AST_PROGRAM.numStatements; i++) {
        ResultType(Nothing, charptr) result = compile(compiler, block->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;
    }

    return Success(Nothing, charptr, {});
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

    // get func arg types
    List(LLVMTypeRef) argTypes = newList(LLVMTypeRef);
    for (size_t i = 0; i < funcDef.args.count; i++) {
        struct AST_ARG_DEF arg = (*get(funcDef.args, i))->data.AST_ARG_DEF;

        ResultType(LLVMTypeRef, charptr) argType = getType(compiler, arg.type->data.AST_TYPE_NAME.name);
        if (argType.error)
            return Error(Nothing, charptr, argType.as.error);

        append(argTypes, argType.as.success);
    }

    // create function
    char* funcName = funcDef.ident->data.AST_IDENTIFIER.ident;
    LLVMTypeRef funcType = LLVMFunctionType(returnType.as.success, argTypes.pointer, argTypes.count, false);
    LLVMValueRef function = LLVMAddFunction(compiler->module,funcName , funcType);

    // set the current function
    LLVMValueRef parentFunc = compiler->currentFunction;
    compiler->currentFunction = function;

    // create function entry
    Estr entryName = CREATE_ESTR(funcName);
    APPEND_ESTR(entryName, "_entry");

    // define func in compilers current env
    defineVar(compiler->env, funcName, function, funcType);

    // create entry and compile func body
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(compiler->context, function, entryName.str);
    LLVMPositionBuilderAtEnd(compiler->builder, entry);

    // setup func environment
    CometEnvironment* oldEnv = compiler->env;
    CometEnvironment* newEnv = newEnvironment(entryName.str, oldEnv);
    compiler->env = newEnv;
    for (size_t i = 0; i < funcDef.args.count; i++) {
        // get arg and alloc space for it
        struct AST_ARG_DEF arg = (*get(funcDef.args, i))->data.AST_ARG_DEF;
        char* argName = arg.ident->data.AST_IDENTIFIER.ident;
        

        LLVMTypeRef argType = *get(argTypes, i);

        LLVMValueRef argValue = LLVMGetParam(function, i);

        LLVMValueRef argPtr = LLVMBuildAlloca(compiler->builder, argType, argName);
        LLVMBuildStore(compiler->builder, argValue, argPtr);

        // add env var for it
        defineVar(compiler->env, argName, argPtr, argType);
    }

    ResultType(Nothing, charptr) bodyResult = compileBlock(compiler, funcDef.program);
    if (bodyResult.error)
        return bodyResult;

    // return to the old function
    compiler->currentFunction = parentFunc;
    compiler->env = oldEnv;
    free(newEnv);

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) visitReturnStatement(CometCompiler* compiler, CometASTNode* node) {
    
    ResultType(CometTypeValuePair, charptr) returnValue = resolveValue(compiler, node->data.AST_RETURN_STATEMENT.expression);
    
    if (returnValue.error)
        return Error(Nothing, charptr, returnValue.as.error);

    LLVMTypeRef funcType = LLVMGlobalGetValueType(compiler->currentFunction);
    LLVMTypeRef returnType = LLVMGetReturnType(funcType);

    if (returnValue.as.success.type != returnType) {
        ResultType(LLVMValueRef, charptr) cast = castToType(compiler->builder, returnValue.as.success.value, returnType);
        if (cast.error)
            return Error(Nothing, charptr, cast.as.error);

        returnValue.as.success.value = cast.as.success;
    }
    
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

    

    ResultType(LLVMTypeRef, charptr) varAssignType = getType(compiler, type);
    if (varAssignType.error)
        return Error(Nothing, charptr, varAssignType.as.error);


    LLVMValueRef ptr = LLVMBuildAlloca(compiler->builder, varAssignType.as.success, name);

    if (value) { // if we set an actual value or didnt assign one
        ResultType(CometTypeValuePair, charptr) typeValuePair = resolveValue(compiler, value);
        if (typeValuePair.error)
            return Error(Nothing, charptr, typeValuePair.as.error);

        if (varAssignType.as.success != typeValuePair.as.success.type) {
            ResultType(LLVMValueRef, charptr) cast = castToType(compiler->builder, typeValuePair.as.success.value, varAssignType.as.success);
            if (cast.error)
                return Error(Nothing, charptr, cast.as.error);

            typeValuePair.as.success.value = cast.as.success;
            typeValuePair.as.success.type = varAssignType.as.success;
        }
        LLVMBuildStore(compiler->builder, typeValuePair.as.success.value, ptr);
    }

    defineVar(compiler->env, name, ptr, varAssignType.as.success);

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
        ResultType(LLVMValueRef, charptr) cast = castToType(compiler->builder, typeValuePair.as.success.value, varRecord->type);
        if (cast.error) {
            Estr errMsg = CREATE_ESTR("Attempt to change type of variable \"");
            APPEND_ESTR(errMsg, name);
            APPEND_ESTR(errMsg, "\" at runtime.");
            return Error(Nothing, charptr, errMsg.str);
        }

        typeValuePair.as.success.value = cast.as.success;
    }

    LLVMBuildStore(compiler->builder, typeValuePair.as.success.value, varRecord->ptr);

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) visitIfStatement(CometCompiler* compiler, CometASTNode* node) {
    CometASTNode* condition = node->data.AST_IF_STATEMENT.expression;
    CometASTNode* consequence = node->data.AST_IF_STATEMENT.program;
    CometASTNode* otherwise = node->data.AST_IF_STATEMENT.elseProgram;

    ResultType(CometTypeValuePair, charptr) result = resolveValue(compiler, condition);
    if (result.error)
        return Error(Nothing, charptr, result.as.error);

    LLVMBasicBlockRef thenBB = LLVMAppendBasicBlockInContext(compiler->context, compiler->currentFunction, "then");
    LLVMBasicBlockRef elseBB = NULL;
    if (otherwise) {
        elseBB = LLVMAppendBasicBlockInContext(compiler->context, compiler->currentFunction, "else");
    }
    LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlockInContext(compiler->context, compiler->currentFunction, "merge");

    if (elseBB) {
        LLVMBuildCondBr(compiler->builder, result.as.success.value, thenBB, elseBB);
    } else {
        LLVMBuildCondBr(compiler->builder, result.as.success.value, thenBB, mergeBB);
    }
    

    // build then block
    LLVMPositionBuilderAtEnd(compiler->builder, thenBB);
    ResultType(Nothing, charptr) bodyResult = compileBlock(compiler, consequence);
    if (bodyResult.error)
        return bodyResult;
    LLVMBuildBr(compiler->builder, mergeBB);

    // build else block
    if (otherwise) {
        LLVMPositionBuilderAtEnd(compiler->builder, elseBB);

        ResultType(Nothing, charptr) elseResult = compileBlock(compiler, otherwise);
        if (elseResult.error)
            return elseResult;
        LLVMBuildBr(compiler->builder, mergeBB);
    }

    // emit merge block
    LLVMPositionBuilderAtEnd(compiler->builder, mergeBB);

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) visitWhileStatement(CometCompiler* compiler, CometASTNode* node) {
    CometASTNode* condition = node->data.AST_WHILE_STATEMENT.expression;
    CometASTNode* body = node->data.AST_WHILE_STATEMENT.program;

    ResultType(CometTypeValuePair, charptr) beforeCheck = resolveValue(compiler, condition);
    if (beforeCheck.error)
        return Error(Nothing, charptr, beforeCheck.as.error);

    LLVMBasicBlockRef whileLoopEntry = LLVMAppendBasicBlockInContext(compiler->context, compiler->currentFunction, "whileLoopEntry");
    LLVMBasicBlockRef whileLoopOtherwise = LLVMAppendBasicBlockInContext(compiler->context, compiler->currentFunction, "whileLoopOtherwise");

    LLVMBuildCondBr(compiler->builder, beforeCheck.as.success.value, whileLoopEntry, whileLoopOtherwise);

    // create loop env
    CometEnvironment* oldEnv = compiler->env;
    CometEnvironment* newEnv = newEnvironment("loopEnv", compiler->env);
    compiler->env = newEnv;

    // build loop body
    LLVMPositionBuilderAtEnd(compiler->builder, whileLoopEntry);
    ResultType(Nothing, charptr) bodyResult = compileBlock(compiler, body);
    if (bodyResult.error)
        return bodyResult;

    ResultType(CometTypeValuePair, charptr) afterCheck = resolveValue(compiler, condition);
    if (afterCheck.error)
        return Error(Nothing, charptr, afterCheck.as.error);

    LLVMBuildCondBr(compiler->builder, afterCheck.as.success.value, whileLoopEntry, whileLoopOtherwise);
    LLVMPositionBuilderAtEnd(compiler->builder, whileLoopOtherwise);

    // reset env
    compiler->env = oldEnv;
    free(newEnv);

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) visitForStatement(CometCompiler* compiler, CometASTNode* node) {
    struct AST_FOR_STATEMENT forLoop = node->data.AST_FOR_STATEMENT;
    char* identName = forLoop.ident->data.AST_IDENTIFIER.ident;

    ResultType(LLVMTypeRef, charptr) varType = getType(compiler, forLoop.type->data.AST_TYPE_NAME.name);
    if (varType.error)
        return Error(Nothing, charptr, varType.as.error);

    ResultType(CometTypeValuePair, charptr) startValue = resolveValue(compiler, forLoop.start);
    if (startValue.error)
        return Error(Nothing, charptr, startValue.as.error);

    ResultType(CometTypeValuePair, charptr) endValue = resolveValue(compiler, forLoop.end);
    if (endValue.error)
        return Error(Nothing, charptr, endValue.as.error);

    ResultType(CometTypeValuePair, charptr) stepValue = resolveValue(compiler, forLoop.step);
    if (stepValue.error)
        return Error(Nothing, charptr, stepValue.as.error);

    LLVMValueRef ptr = LLVMBuildAlloca(compiler->builder, varType.as.success, identName);
    LLVMBuildStore(compiler->builder, startValue.as.success.value, ptr);
    defineVar(compiler->env, identName, ptr, varType.as.success);

    LLVMBasicBlockRef forLoopEntry = LLVMAppendBasicBlockInContext(compiler->context, compiler->currentFunction, "forLoopEntry");
    LLVMBasicBlockRef forLoopOtherwise = LLVMAppendBasicBlockInContext(compiler->context, compiler->currentFunction, "forLoopOtherwise");

    LLVMValueRef isNotEqual = LLVMBuildICmp(compiler->builder, LLVMIntNE, startValue.as.success.value, endValue.as.success.value, "tmp");
    LLVMBuildCondBr(compiler->builder, isNotEqual, forLoopEntry, forLoopOtherwise);

    LLVMPositionBuilderAtEnd(compiler->builder, forLoopEntry);
    ResultType(Nothing, charptr) bodyResult = compileBlock(compiler, forLoop.program);
    if (bodyResult.error)
        return bodyResult;

    

    LLVMValueRef iteratorValue = LLVMBuildLoad2(compiler->builder, varType.as.success, ptr, identName);
    LLVMValueRef afterAdd = LLVMBuildAdd(compiler->builder, iteratorValue, stepValue.as.success.value, "tmp");
    isNotEqual = LLVMBuildICmp(compiler->builder, LLVMIntNE, afterAdd, endValue.as.success.value, "tmp");
    LLVMBuildStore(compiler->builder, afterAdd, ptr);

    LLVMBuildCondBr(compiler->builder, isNotEqual, forLoopEntry, forLoopOtherwise);

    LLVMPositionBuilderAtEnd(compiler->builder, forLoopOtherwise);

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

        LLVMValuePair verified = verifyInts(compiler, left.as.success.value, right.as.success.value);
        left.as.success.value = verified.a;
        right.as.success.value = verified.b;

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

            // conditionals
            case CT_EQ_EQ: {
                type = getType(compiler, "bool");
                value = LLVMBuildICmp(compiler->builder, LLVMIntEQ, left.as.success.value, right.as.success.value, "tmp");
                break;
            }
            case CT_LT: {
                type = getType(compiler, "bool");
                value = LLVMBuildICmp(compiler->builder, LLVMIntSLT, left.as.success.value, right.as.success.value, "tmp");
                break;
            }
            case CT_LTE: {
                type = getType(compiler, "bool");
                value = LLVMBuildICmp(compiler->builder, LLVMIntULE, left.as.success.value, right.as.success.value, "tmp");
                break;
            }
            case CT_GT: {
                type = getType(compiler, "bool");
                value = LLVMBuildICmp(compiler->builder, LLVMIntSGT, left.as.success.value, right.as.success.value, "tmp");
                break;
            }
            case CT_GTE: {
                type = getType(compiler, "bool");
                value = LLVMBuildICmp(compiler->builder, LLVMIntUGE, left.as.success.value, right.as.success.value, "tmp");
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

ResultType(CometTypeValuePair, charptr) visitFuncCall(CometCompiler* compiler, CometASTNode* node) {
    struct AST_FUNC_CALL funcCall = node->data.AST_FUNC_CALL;
    char* funcName = funcCall.ident->data.AST_IDENTIFIER.ident;

    Record* funcRecord = lookup(compiler->env, funcName);
    if (!funcRecord) {
        Estr errMsg = CREATE_ESTR("Attempt to call undefined function \"");
        APPEND_ESTR(errMsg, funcName);
        APPEND_ESTR(errMsg, "\"");

        return Error(CometTypeValuePair, charptr, errMsg.str);
    }

    List(LLVMValueRef) argValues = newList(LLVMValueRef);
    for (size_t i = 0; i < funcCall.args.count; i++) {
        ResultType(CometTypeValuePair, charptr) arg = resolveValue(compiler, *get(funcCall.args, i));
        if (arg.error)
            return arg;

        append(argValues, arg.as.success.value);
    }

    LLVMValueRef returnValue = LLVMBuildCall2(compiler->builder, funcRecord->type, funcRecord->ptr, argValues.pointer, argValues.count, funcName);
    CometTypeValuePair result = {
        .value = returnValue,
        .type = LLVMGetReturnType(funcRecord->type)
    };

    return Success(CometTypeValuePair, charptr, result);
}
 
// -- MAIN --//
ResultType(Nothing, charptr) compileAST(CometCompiler* compiler, CometASTNode* root, const char* outputName) {
    ResultType(Nothing, charptr) compilerResult = compile(compiler, root);
    if (compilerResult.error)
        return compilerResult;

    LLVMPrintModuleToFile(compiler->module, outputName, NULL);

    return Success(Nothing, charptr, {});
}

void defineInternalConstants(CometCompiler* compiler) {
    // define print
    LLVMTypeRef i8ptr = LLVMPointerType(LLVMInt8TypeInContext(compiler->context), 0);
    LLVMTypeRef printfArgs[] = { i8ptr };
    LLVMTypeRef printfType = LLVMFunctionType(
        LLVMInt32TypeInContext(compiler->context),
        printfArgs,
        1,
        true
    );

    LLVMValueRef printfFunc = LLVMAddFunction(compiler->module, "printf", printfType);

    defineVar(compiler->env, "print", printfFunc, printfType);
}

ResultType(cometCompilerPtr, charptr) createCompiler(CometParser* parser) {
    CometCompiler* newCompiler = malloc(sizeof(CometCompiler));
    if (!newCompiler)
        return Error(cometCompilerPtr, charptr, "createCompiler: failed to allocate memory for compiler struct!");

    newCompiler->context = LLVMContextCreate();
    newCompiler->module = LLVMModuleCreateWithNameInContext("main", newCompiler->context);
    newCompiler->builder = LLVMCreateBuilderInContext(newCompiler->context);
    newCompiler->currentFunction = NULL;

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

    // define internal constants
    defineInternalConstants(newCompiler);

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
        case AST_IF_STATEMENT:
            return visitIfStatement(compiler, node);
        case AST_WHILE_STATEMENT:
            return visitWhileStatement(compiler, node);
        case AST_FOR_STATEMENT:
            return visitForStatement(compiler, node);

        case AST_INFIX_EXPRESSION: {
            ResultType(CometTypeValuePair, charptr) result = visitInfixExpression(compiler, node);
            if (result.error)
                return Error(Nothing, charptr, result.as.error);

            return Success(Nothing, charptr, {});
        }
        case AST_FUNC_CALL: {
            ResultType(CometTypeValuePair, charptr) result = visitFuncCall(compiler, node);
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