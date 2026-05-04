#include "compiler.h"
#include "ast.h"
#include "environment.h"
#include "lexer.h"
#include "parser.h"
#include "struct.h"
#include "token.h"
#include "util.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <llvm-c/Analysis.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* BUILT_IN_TYPES[] = {
    "small",
    "int",
    "big",

    "float",
    "double",

    "bool",

    "void"
};


// -- HELPER FUNCTIONS -- //
ResultType(LLVMTypeRef, charptr) getType(CometCompiler* compiler, char* typeName) {
    for (size_t i = 0; i < compiler->typeMap.count; i++) {
        CometLLVMTypePair type = *get(compiler->typeMap, i);

        if (strcmp(type.typeName, typeName) == 0) {
            return Success(LLVMTypeRef, charptr, type.llvmType);
        }
    }

    Estr errMsg = CREATE_ESTR("The type \"");
    APPEND_ESTR(errMsg, typeName);
    APPEND_ESTR(errMsg, "\" was not found!");
    return Error(LLVMTypeRef, charptr, errMsg.str);
}

StructInfo* getStruct(CometCompiler* compiler, LLVMTypeRef structType) {
    for (size_t i = 0; i < compiler->structs.count; i++) {
        StructInfo* structInfo = get(compiler->structs, i);

        if (structInfo->llvmType == structType) {
            return structInfo;
        }
    }

    return NULL;
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

    char* srcTypeStr = LLVMPrintTypeToString(srcType);
    char* targetTypeStr = LLVMPrintTypeToString(targetType);

    Estr errMsg = CREATE_ESTR("Cannot cast ");
    APPEND_ESTR(errMsg, srcTypeStr);
    APPEND_ESTR(errMsg, " to ");
    APPEND_ESTR(errMsg, targetTypeStr);
    APPEND_ESTR(errMsg, ".");
    return Error(LLVMValueRef, charptr, errMsg.str);
}

ResultType(CometValue, charptr) convertString(CometCompiler* compiler, char* str) {
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

    CometValue res = {
        .value = ptr,
        .type = LLVMTypeOf(ptr)
    };
    return Success(CometValue, charptr, res);
}

ResultType(CometValue, charptr) visitInfixExpression(CometCompiler* compiler, CometASTNode* node);
ResultType(CometValue, charptr) visitFuncCall(CometCompiler* compiler, CometASTNode* node);
ResultType(CometValue, charptr) visitNewStatement(CometCompiler* compiler, CometASTNode* node);
ResultType(CometValue, charptr) resolveValue(CometCompiler* compiler, CometASTNode* node) {
    CometValue res;

    switch (node->nodeType) {
        case AST_INT: {
            ResultType(LLVMTypeRef, charptr) intType = getType(compiler, "int");
            if (intType.error)
                return Error(CometValue, charptr, intType.as.error);

            res = (CometValue){
                LLVMConstInt(intType.as.success, node->data.AST_INT.number, false),
                intType.as.success,
                .isPointer = false
            };
            break;
        }

        case AST_DOUBLE: {
            ResultType(LLVMTypeRef, charptr) doubleType = getType(compiler, "double");
            if (doubleType.error)
                return Error(CometValue, charptr, doubleType.as.error);

            res = (CometValue){
                LLVMConstReal(doubleType.as.success, node->data.AST_DOUBLE.number),
                doubleType.as.success,
                .isPointer = false
            };
            break;
        }

        case AST_STRING: {
            ResultType(CometValue, charptr) stringType = convertString(compiler, node->data.AST_STRING.value);
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

        case AST_NEW_STATEMENT: {
            return visitNewStatement(compiler, node);
        }

        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(compiler->env, varName);
            if (!varRecord) {
                Estr errMsg = CREATE_ESTR("Undefined variable \"");
                APPEND_ESTR(errMsg, varName);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometValue, charptr, errMsg.str);
            }

            CometValue result = {
                .value = LLVMBuildLoad2(compiler->builder, varRecord->type, varRecord->ptr, varName),
                .type = varRecord->type,
                .isPointer = false
            };
            return Success(CometValue, charptr, result);

            break;
        }

        default:
            return Error(CometValue, charptr, "Unkown expression type.");
    }

    return Success(CometValue, charptr, res);
}

ResultType(CometValue, charptr) resolvePointerValue(CometCompiler* compiler, CometASTNode* node) {
    switch (node->nodeType) {
        case AST_INFIX_EXPRESSION: {

            switch (node->data.AST_INFIX_EXPRESSION.op.type) {
                case CT_DOT: {
                    ResultType(CometValue, charptr) left = resolvePointerValue(compiler, node->data.AST_INFIX_EXPRESSION.left);
                    if (left.error) return Error(CometValue, charptr, left.as.error);

                    if (node->data.AST_INFIX_EXPRESSION.right->nodeType != AST_IDENTIFIER) {
                        return Error(CometValue, charptr, "Huh?");
                    }

                    StructInfo* structInfo = getStruct(compiler, left.as.success.type);
                    if (structInfo == NULL) {
                        return Error(CometValue, charptr, "Attempt to get field of something that isn't a struct.");
                    }

                    char* fieldName = node->data.AST_INFIX_EXPRESSION.right->data.AST_IDENTIFIER.ident;
                    StructField* fieldInfo = findField(*structInfo, fieldName);
                    if (fieldInfo == NULL) {
                        Estr errMsg = CREATE_ESTR("The struct \"");
                        APPEND_ESTR(errMsg, structInfo->name);
                        APPEND_ESTR(errMsg, "\" does not have the field \"");
                        APPEND_ESTR(errMsg, fieldName);
                        APPEND_ESTR(errMsg, "\"");

                        return Error(CometValue, charptr, errMsg.str);
                    }

                    LLVMValueRef zero   = LLVMConstInt(LLVMInt32TypeInContext(compiler->context), 0, false);
                    LLVMValueRef index  = LLVMConstInt(LLVMInt32TypeInContext(compiler->context), fieldInfo->index, false);

                    Estr ptrName = CREATE_ESTR(structInfo->name);
                    APPEND_ESTR(ptrName, "_access");
                    LLVMValueRef ptr = LLVMBuildGEP2(
                        compiler->builder,
                        left.as.success.type,
                        left.as.success.pointer,
                        (LLVMValueRef[]){ zero, index },
                        2,
                        ptrName.str
                    );

                    /*Estr valueName = CREATE_ESTR(fieldName);
                    APPEND_ESTR(valueName, "_field");

                    LLVMValueRef value = LLVMBuildLoad2(
                        compiler->builder,
                        fieldInfo->llvmType,
                        ptr,
                        valueName.str
                    );*/
                    
                    //printf("%s is struct = %d\n", fieldName, LLVMGetTypeKind(fieldInfo->llvmType) == LLVMStructTypeKind);
                    bool isPointer = LLVMGetTypeKind(fieldInfo->llvmType) == LLVMStructTypeKind;

                    //Estr tempName = CREATE_ESTR(fieldName);
                    //APPEND_ESTR(tempName, "_tmp");

                    //LLVMValueRef temp = LLVMBuildAlloca(compiler->builder, fieldInfo->llvmType, tempName.str);
                    //LLVMBuildStore(compiler->builder, value, temp);

                    CometValue result = (CometValue){
                        .pointer = ptr,
                        .type = fieldInfo->llvmType,
                        .isPointer = isPointer
                    };
                    return Success(CometValue, charptr, result);
                }

                default:
                    return Error(CometValue, charptr, "Attempt to use dot operator on something that isn't a struct.");
            }
            return visitInfixExpression(compiler, node);
        }

        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(compiler->env, varName);
            if (!varRecord) {
                Estr errMsg = CREATE_ESTR("Undefined variable \"");
                APPEND_ESTR(errMsg, varName);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometValue, charptr, errMsg.str);
            }

            CometValue result = {
                .pointer = varRecord->ptr,
                .type = varRecord->type,
                .isPointer = true
            };
            return Success(CometValue, charptr, result);
        }

        default:
            return Error(CometValue, charptr, "Unkown pointer type.");
    }
}

ResultType(int, charptr) compileBlock(CometCompiler* compiler, CometASTNode* block) {
    bool doesReturn = false;

    for (size_t i = 0; i < block->data.AST_PROGRAM.numStatements; i++) {
        ResultType(int, charptr) result = compile(compiler, block->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return Error(int, charptr, result.as.error);

        if (!doesReturn && result.as.success) {
            doesReturn = true;
        } 
    }

    return Success(int, charptr, doesReturn);
}

// -- STATEMENTS -- //
ResultType(int, charptr) visitProgram(CometCompiler* compiler, CometASTNode* node) {
    bool doesReturn = false;

    for (size_t i = 0; i < node->data.AST_PROGRAM.numStatements; i++) {
        ResultType(int, charptr) result = compile(compiler, node->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;

        if (!doesReturn && result.as.success) {
            doesReturn = true;
        }
    }

    return Success(int, charptr, doesReturn);
}

ResultType(int, charptr) visitFuncDefStatement(CometCompiler* compiler, CometASTNode* node) {
    struct AST_FUNC_DEF_STATEMENT funcDef = node->data.AST_FUNC_DEF_STATEMENT;
    bool doesReturn = false;

    ResultType(LLVMTypeRef, charptr) returnType = getType(compiler, funcDef.returnType->data.AST_IDENTIFIER.ident);
    if (returnType.error)
        return Error(int, charptr, returnType.as.error);

    // get func arg types
    List(LLVMTypeRef) argTypes = newList(LLVMTypeRef);
    for (size_t i = 0; i < funcDef.args.count; i++) {
        struct AST_ARG_DEF arg = (*get(funcDef.args, i))->data.AST_ARG_DEF;

        ResultType(LLVMTypeRef, charptr) argType = getType(compiler, arg.type->data.AST_IDENTIFIER.ident);
        if (argType.error)
            return Error(int, charptr, argType.as.error);

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

    if (!funcDef.isInline) {
        ResultType(int, charptr) bodyResult = compileBlock(compiler, funcDef.program);
        if (bodyResult.error)
            return Error(int, charptr, bodyResult.as.error);
        doesReturn = bodyResult.as.success;

        // make sure the function returns
        if (!doesReturn) {
            if (LLVMGetTypeKind(returnType.as.success) == LLVMVoidTypeKind) {
                LLVMBuildRetVoid(compiler->builder);
            } else {
                Estr errMsg = CREATE_ESTR("Non-void function \"");
                APPEND_ESTR(errMsg, funcName);
                APPEND_ESTR(errMsg, "\" does not return!");
                return Error(int, charptr, errMsg.str);
            }
        }
    } else { // is inline func
        ResultType(CometValue, charptr) inlineBody = visitInfixExpression(compiler, funcDef.inlineExpr);
        if (inlineBody.error)
            return Error(int, charptr, inlineBody.as.error);

        if (inlineBody.as.success.isPointer) {
            LLVMBuildRet(compiler->builder, inlineBody.as.success.pointer);
        } else {
            LLVMBuildRet(compiler->builder, inlineBody.as.success.value);
        }
    }

    // return to the old function
    compiler->currentFunction = parentFunc;
    compiler->env = oldEnv;
    free(newEnv);

    return Success(int, charptr, doesReturn);
}

ResultType(int, charptr) visitReturnStatement(CometCompiler* compiler, CometASTNode* node) {
    
    ResultType(CometValue, charptr) returnValue = resolveValue(compiler, node->data.AST_RETURN_STATEMENT.expression);
    
    if (returnValue.error)
        return Error(int, charptr, returnValue.as.error);

    LLVMTypeRef funcType = LLVMGlobalGetValueType(compiler->currentFunction);
    LLVMTypeRef returnType = LLVMGetReturnType(funcType);


    if (returnValue.as.success.isPointer &&
               LLVMPointerType(returnValue.as.success.type, 0) != returnType) {
        
        Estr errMsg = CREATE_ESTR("Attempt to return ptr ");
        APPEND_ESTR(errMsg, LLVMPrintTypeToString(returnValue.as.success.type));
        APPEND_ESTR(errMsg, " from function of return type ")
        APPEND_ESTR(errMsg, LLVMPrintTypeToString(returnType));
        APPEND_ESTR(errMsg, ".");
        
        return Error(int, charptr, errMsg.str);
    } else if (returnValue.as.success.type != returnType) {
        ResultType(LLVMValueRef, charptr) cast = castToType(compiler->builder, returnValue.as.success.value, returnType);
        if (cast.error)
            return Error(int, charptr, cast.as.error);

        returnValue.as.success.value = cast.as.success;
    }
    
    if (returnValue.as.success.isPointer) {
        LLVMBuildRet(compiler->builder, returnValue.as.success.pointer);
    } else {
        LLVMBuildRet(compiler->builder, returnValue.as.success.value);
    }
    

    return Success(int, charptr, true);
}

ResultType(int, charptr) visitExpressionStatement(CometCompiler* compiler, CometASTNode* node) {
    return compile(compiler, node->data.AST_EXPRESSION_STATEMENT.expression);
}

ResultType(int, charptr) visitAssignStatement(CometCompiler* compiler, CometASTNode* node) {
    char* name = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;
    CometASTNode* value = node->data.AST_ASSIGN_STATEMENT.expression;
    char* type = node->data.AST_ASSIGN_STATEMENT.type->data.AST_IDENTIFIER.ident;

    Record* varRecord = lookup(compiler->env, name);
    if (varRecord) {
        Estr errMsg = CREATE_ESTR("Redeclaration of variable \"");
        APPEND_ESTR(errMsg, name);
        APPEND_ESTR(errMsg, "\"");
        return Error(int, charptr, errMsg.str);
    }

    

    ResultType(LLVMTypeRef, charptr) varAssignType = getType(compiler, type);
    if (varAssignType.error)
        return Error(int, charptr, varAssignType.as.error);


    LLVMValueRef ptr = LLVMBuildAlloca(compiler->builder, varAssignType.as.success, name);

    if (value) { // if we set an actual value or didnt assign one
        ResultType(CometValue, charptr) typeValuePair = resolveValue(compiler, value);
        if (typeValuePair.error)
            return Error(int, charptr, typeValuePair.as.error);

        if (varAssignType.as.success != typeValuePair.as.success.type) {
            ResultType(LLVMValueRef, charptr) cast = castToType(compiler->builder, typeValuePair.as.success.value, varAssignType.as.success);
            if (cast.error)
                return Error(int, charptr, cast.as.error);

            typeValuePair.as.success.value = cast.as.success;
            typeValuePair.as.success.type = varAssignType.as.success;
        }

        if (typeValuePair.as.success.isPointer) {
            LLVMBuildStore(compiler->builder, typeValuePair.as.success.pointer, ptr);
        } else {
            LLVMBuildStore(compiler->builder, typeValuePair.as.success.value, ptr);
        }
        
    }

    defineVar(compiler->env, name, ptr, varAssignType.as.success);

    return Success(int, charptr, false);
}

ResultType(int, charptr) visitReassignStatement(CometCompiler* compiler, CometASTNode* node) {
    CometASTNode* left = node->data.AST_REASSIGN_STATEMENT.ident;
    CometASTNode* value = node->data.AST_REASSIGN_STATEMENT.expression;

    

    ResultType(CometValue, charptr) typeValuePair = resolveValue(compiler, value);
        if (typeValuePair.error)
            return Error(int, charptr, typeValuePair.as.error);

    if (left->nodeType == AST_INFIX_EXPRESSION) { // struct reassign
        ResultType(CometValue, charptr) structToChange = resolvePointerValue(compiler, left->data.AST_INFIX_EXPRESSION.left);
        if (structToChange.error)
            return Error(int, charptr, structToChange.as.error);

        char* fieldName = left->data.AST_INFIX_EXPRESSION.right->data.AST_IDENTIFIER.ident;
        StructInfo* structInfo = getStruct(compiler, structToChange.as.success.type);
        if (structInfo == NULL) {
            return Error(int, charptr, "Attempt to get field of something that isn't a struct.");
        }

        StructField* fieldInfo = findField(*structInfo, fieldName);
        if (fieldInfo == NULL) {
            Estr errMsg = CREATE_ESTR("The struct \"");
            APPEND_ESTR(errMsg, structInfo->name);
            APPEND_ESTR(errMsg, "\" does not have the field \"");
            APPEND_ESTR(errMsg, fieldName);
            APPEND_ESTR(errMsg, "\"");

            return Error(int, charptr, errMsg.str);
        }

        LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(compiler->context), 0, false);
        LLVMValueRef index = LLVMConstInt(LLVMInt32TypeInContext(compiler->context), fieldInfo->index, false);

        Estr ptrName = CREATE_ESTR(fieldName);
        APPEND_ESTR(ptrName, "FieldPtr");

        //pprintf("structToChange = %s\n", LLVMPrintValueToString(structToChange.as.success.pointer));

        LLVMValueRef ptr = LLVMBuildGEP2(
            compiler->builder,
            structToChange.as.success.type,
            structToChange.as.success.pointer,
            (LLVMValueRef[]){ zero, index },
            2,
            ptrName.str
        );

        LLVMBuildStore(
            compiler->builder,
            typeValuePair.as.success.value,
            ptr
        );

    } else { // var reassign
        char* name = left->data.AST_IDENTIFIER.ident;
        Record* varRecord = lookup(compiler->env, name);
        if (!varRecord) {
            Estr errMsg = CREATE_ESTR("Undefined variable \"");
            APPEND_ESTR(errMsg, name);
            APPEND_ESTR(errMsg, "\"");
            return Error(int, charptr, errMsg.str);
        }

        if (typeValuePair.as.success.type != varRecord->type) {
            ResultType(LLVMValueRef, charptr) cast = castToType(compiler->builder, typeValuePair.as.success.value, varRecord->type);
            if (cast.error) {
                Estr errMsg = CREATE_ESTR("Attempt to change type of variable \"");
                APPEND_ESTR(errMsg, name);
                APPEND_ESTR(errMsg, "\" at runtime.");
                return Error(int, charptr, errMsg.str);
            }

            typeValuePair.as.success.value = cast.as.success;
        }

        LLVMBuildStore(compiler->builder, typeValuePair.as.success.value, varRecord->ptr);
    }
    

    

    return Success(int, charptr, false);
}

ResultType(int, charptr) visitIfStatement(CometCompiler* compiler, CometASTNode* node) {
    CometASTNode* condition = node->data.AST_IF_STATEMENT.expression;
    CometASTNode* consequence = node->data.AST_IF_STATEMENT.program;
    CometASTNode* otherwise = node->data.AST_IF_STATEMENT.elseProgram;

    ResultType(CometValue, charptr) result = resolveValue(compiler, condition);
    if (result.error)
        return Error(int, charptr, result.as.error);

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
    ResultType(int, charptr) bodyResult = compileBlock(compiler, consequence);
    if (bodyResult.error)
        return Error(int, charptr, bodyResult.as.error);
    LLVMBuildBr(compiler->builder, mergeBB);

    // build else block
    bool elseReturns = false;
    if (otherwise) {
        LLVMPositionBuilderAtEnd(compiler->builder, elseBB);

        ResultType(int, charptr) elseResult = compileBlock(compiler, otherwise);
        if (elseResult.error)
            return Error(int, charptr, elseResult.as.error);
        LLVMBuildBr(compiler->builder, mergeBB);
        elseReturns = elseResult.as.success;
    }

    // emit merge block
    LLVMPositionBuilderAtEnd(compiler->builder, mergeBB);

    return Success(int, charptr, bodyResult.as.success && elseReturns);
}

ResultType(int, charptr) visitWhileStatement(CometCompiler* compiler, CometASTNode* node) {
    CometASTNode* condition = node->data.AST_WHILE_STATEMENT.expression;
    CometASTNode* body = node->data.AST_WHILE_STATEMENT.program;

    ResultType(CometValue, charptr) beforeCheck = resolveValue(compiler, condition);
    if (beforeCheck.error)
        return Error(int, charptr, beforeCheck.as.error);

    ResultType(LLVMTypeRef, charptr) boolType = getType(compiler, "bool");
    if (boolType.error)
        return Error(int, charptr, boolType.as.error);

    if (beforeCheck.as.success.type != boolType.as.success) {
        ResultType(LLVMValueRef, charptr) casted = castToType(compiler->builder, beforeCheck.as.success.isPointer ? beforeCheck.as.success.pointer : beforeCheck.as.success.value, boolType.as.success);
        if (casted.error)
            return Error(int, charptr, casted.as.error);

        if (beforeCheck.as.success.isPointer) {
            beforeCheck.as.success.pointer = casted.as.success;
        } else {
            beforeCheck.as.success.value = casted.as.success;
        }
        beforeCheck.as.success.type = boolType.as.success;
        
    }

    LLVMBasicBlockRef whileLoopEntry = LLVMAppendBasicBlockInContext(compiler->context, compiler->currentFunction, "whileLoopEntry");
    LLVMBasicBlockRef whileLoopOtherwise = LLVMAppendBasicBlockInContext(compiler->context, compiler->currentFunction, "whileLoopOtherwise");

    LLVMBuildCondBr(compiler->builder, beforeCheck.as.success.value, whileLoopEntry, whileLoopOtherwise);

    // create loop env
    CometEnvironment* oldEnv = compiler->env;
    CometEnvironment* newEnv = newEnvironment("loopEnv", compiler->env);
    compiler->env = newEnv;

    // build loop body
    LLVMPositionBuilderAtEnd(compiler->builder, whileLoopEntry);
    ResultType(int, charptr) bodyResult = compileBlock(compiler, body);
    if (bodyResult.error)
        return Error(int, charptr, bodyResult.as.error);

    ResultType(CometValue, charptr) afterCheck = resolveValue(compiler, condition);
    if (afterCheck.error)
        return Error(int, charptr, afterCheck.as.error);

    LLVMBuildCondBr(compiler->builder, afterCheck.as.success.value, whileLoopEntry, whileLoopOtherwise);
    LLVMPositionBuilderAtEnd(compiler->builder, whileLoopOtherwise);

    // reset env
    compiler->env = oldEnv;
    free(newEnv);

    return Success(int, charptr, false);
}

ResultType(int, charptr) visitForStatement(CometCompiler* compiler, CometASTNode* node) {
    struct AST_FOR_STATEMENT forLoop = node->data.AST_FOR_STATEMENT;
    char* identName = forLoop.ident->data.AST_IDENTIFIER.ident;

    ResultType(LLVMTypeRef, charptr) varType = getType(compiler, forLoop.type->data.AST_IDENTIFIER.ident);
    if (varType.error)
        return Error(int, charptr, varType.as.error);

    ResultType(CometValue, charptr) startValue = resolveValue(compiler, forLoop.start);
    if (startValue.error)
        return Error(int, charptr, startValue.as.error);

    ResultType(CometValue, charptr) endValue = resolveValue(compiler, forLoop.end);
    if (endValue.error)
        return Error(int, charptr, endValue.as.error);

    ResultType(CometValue, charptr) stepValue = resolveValue(compiler, forLoop.step);
    if (stepValue.error)
        return Error(int, charptr, stepValue.as.error);

    LLVMValueRef ptr = LLVMBuildAlloca(compiler->builder, varType.as.success, identName);
    LLVMBuildStore(compiler->builder, startValue.as.success.value, ptr);
    defineVar(compiler->env, identName, ptr, varType.as.success);

    LLVMBasicBlockRef forLoopEntry = LLVMAppendBasicBlockInContext(compiler->context, compiler->currentFunction, "forLoopEntry");
    LLVMBasicBlockRef forLoopOtherwise = LLVMAppendBasicBlockInContext(compiler->context, compiler->currentFunction, "forLoopOtherwise");

    LLVMValueRef isNotEqual = LLVMBuildICmp(compiler->builder, LLVMIntNE, startValue.as.success.value, endValue.as.success.value, "");
    LLVMBuildCondBr(compiler->builder, isNotEqual, forLoopEntry, forLoopOtherwise);

    LLVMPositionBuilderAtEnd(compiler->builder, forLoopEntry);
    ResultType(int, charptr) bodyResult = compileBlock(compiler, forLoop.program);
    if (bodyResult.error)
        return Error(int, charptr, bodyResult.as.error);

    

    LLVMValueRef iteratorValue = LLVMBuildLoad2(compiler->builder, varType.as.success, ptr, identName);
    LLVMValueRef afterAdd = LLVMBuildAdd(compiler->builder, iteratorValue, stepValue.as.success.value, "");
    isNotEqual = LLVMBuildICmp(compiler->builder, LLVMIntNE, afterAdd, endValue.as.success.value, "");
    LLVMBuildStore(compiler->builder, afterAdd, ptr);

    LLVMBuildCondBr(compiler->builder, isNotEqual, forLoopEntry, forLoopOtherwise);

    LLVMPositionBuilderAtEnd(compiler->builder, forLoopOtherwise);

    return Success(int, charptr, false);
}

ResultType(int, charptr) visitConstructorDefStatement(CometCompiler* compiler, CometASTNode* node, LLVMTypeRef structType, char* structName) {
    struct AST_CONSTRUCTOR_DEF funcDef = node->data.AST_CONSTRUCTOR_DEF;

    

    // get func arg types
    List(LLVMTypeRef) argTypes = newList(LLVMTypeRef);

    // add self to args
    append(argTypes, LLVMPointerType(structType, 0));

    for (size_t i = 0; i < funcDef.args.count; i++) {
        struct AST_ARG_DEF arg = (*get(funcDef.args, i))->data.AST_ARG_DEF;

        ResultType(LLVMTypeRef, charptr) argType = getType(compiler, arg.type->data.AST_IDENTIFIER.ident);
        if (argType.error)
            return Error(int, charptr, argType.as.error);

        append(argTypes, argType.as.success);
    }

    // gen constructor name
    Estr funcNameEstr = CREATE_ESTR(structName);
    APPEND_ESTR(funcNameEstr, "_CONSTRUCTOR");
    char* funcName = funcNameEstr.str;

    // create function
    ResultType(LLVMTypeRef, charptr) voidType = getType(compiler, "void");
    if (voidType.error)
        return Error(int, charptr, voidType.as.error);

    LLVMTypeRef funcType = LLVMFunctionType(voidType.as.success, argTypes.pointer, argTypes.count, false);
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

    // allocate self
    LLVMValueRef selfValue = LLVMGetParam(function, 0);
    //LLVMValueRef selfPtr = LLVMBuildAlloca(compiler->builder, structType, "self");
    //LLVMBuildStore(compiler->builder, selfValue, selfPtr);
    defineVar(compiler->env, "self", selfValue, structType);

    

    // allocate rest of args
    for (size_t i = 0; i < funcDef.args.count; i++) {
        // get arg and alloc space for it
        struct AST_ARG_DEF arg = (*get(funcDef.args, i))->data.AST_ARG_DEF;
        char* argName = arg.ident->data.AST_IDENTIFIER.ident;
        

        LLVMTypeRef argType = *get(argTypes, i+1);

        LLVMValueRef argValue = LLVMGetParam(function, i+1);

        LLVMValueRef argPtr = LLVMBuildAlloca(compiler->builder, argType, argName);
        LLVMBuildStore(compiler->builder, argValue, argPtr);

        

        // add env var for it
        defineVar(compiler->env, argName, argPtr, argType);
    }

    ResultType(int, charptr) bodyResult = compileBlock(compiler, funcDef.program);
    if (bodyResult.error)
        return Error(int, charptr, bodyResult.as.error);
    if (bodyResult.as.success) { // the user put a return statement in the constructor for some reason

    }

    LLVMBuildRetVoid(compiler->builder);

    // return to the old function
    compiler->currentFunction = parentFunc;
    compiler->env = oldEnv;
    free(newEnv);

    return Success(int, charptr, false);
}

ResultType(int, charptr) visitStructDefStatement(CometCompiler* compiler, CometASTNode* node) {
    struct AST_STRUCT_DEF_STATEMENT structDef = node->data.AST_STRUCT_DEF_STATEMENT;
    char* structName = structDef.ident->data.AST_IDENTIFIER.ident;

    List(StructField) structInfoFields = newList(StructField);
    List(LLVMTypeRef) fieldTypes = newList(LLVMTypeRef);

    for (size_t i = 0; i < structDef.fieldDefs.count; i++) {
        CometASTNode* fieldNode = (*get(structDef.fieldDefs, i));
        CometASTNode* fieldType = fieldNode->data.AST_ASSIGN_STATEMENT.type;

        ResultType(LLVMTypeRef, charptr) llvmFieldType = getType(compiler, fieldType->data.AST_IDENTIFIER.ident);
        if (llvmFieldType.error)
            return Error(int, charptr, llvmFieldType.as.error);

        append(fieldTypes, llvmFieldType.as.success);

        StructField fieldInfo = {
            .index = i,
            .llvmType = llvmFieldType.as.success,
            .name = fieldNode->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident,
            .isPointer = false
        };
        append(structInfoFields, fieldInfo);
    }

    

    LLVMTypeRef structType = LLVMStructCreateNamed(compiler->context, structName);
    LLVMStructSetBody(structType, fieldTypes.pointer, fieldTypes.count, false);
    
    StructInfo structInfo = {
        .llvmType = structType,
        .name = structName,
        .fields = structInfoFields
    };
    append(compiler->structs, structInfo);

    CometLLVMTypePair newPair = {
        .typeName = structDef.ident->data.AST_IDENTIFIER.ident,
        .llvmType = structType
    };

    if (structDef.constructor) {
        ResultType(int, charptr) constructorResult = visitConstructorDefStatement(compiler, structDef.constructor, structType, structName);
        if (constructorResult.error)
            return constructorResult;
    }

    append(compiler->typeMap, newPair);

    return Success(int, charptr, false);
}

// -- EXPRESSIONS -- //
ResultType(CometValue, charptr) visitInfixExpression(CometCompiler* compiler, CometASTNode* node) {
    CometToken op = node->data.AST_INFIX_EXPRESSION.op;

    ResultType(CometValue, charptr) left = resolveValue(compiler, node->data.AST_INFIX_EXPRESSION.left);
    if (left.error) return Error(CometValue, charptr, left.as.error);
    ResultType(CometValue, charptr) right = resolveValue(compiler, node->data.AST_INFIX_EXPRESSION.right);


    // performing an operation on two ints
    ResultType(LLVMTypeRef, charptr) type;
    LLVMValueRef value;
    bool isPointer = false;

    if (LLVMGetTypeKind(left.as.success.type) == LLVMStructTypeKind) {
        switch (op.type) {
            case CT_DOT: {
                if (!left.as.success.isPointer) {
                    left = resolvePointerValue(compiler, node->data.AST_INFIX_EXPRESSION.left);
                    if (left.error) return Error(CometValue, charptr, left.as.error);
                }

                StructInfo* structInfo = getStruct(compiler, left.as.success.type);
                if (structInfo == NULL) {
                    return Error(CometValue, charptr, "Attempt to get field of something that isn't a struct.");
                }

                char* fieldName = node->data.AST_INFIX_EXPRESSION.right->data.AST_IDENTIFIER.ident;
                StructField* fieldInfo = findField(*structInfo, fieldName);
                if (fieldInfo == NULL) {
                    Estr errMsg = CREATE_ESTR("The struct \"");
                    APPEND_ESTR(errMsg, structInfo->name);
                    APPEND_ESTR(errMsg, "\" does not have the field \"");
                    APPEND_ESTR(errMsg, fieldName);
                    APPEND_ESTR(errMsg, "\"");

                    return Error(CometValue, charptr, errMsg.str);
                }

                bool fieldIsStruct = LLVMGetTypeKind(fieldInfo->llvmType) == LLVMStructTypeKind;

                if (fieldIsStruct) {
                    return resolvePointerValue(compiler, node);
                }

                LLVMValueRef zero   = LLVMConstInt(LLVMInt32TypeInContext(compiler->context), 0, false);
                LLVMValueRef index  = LLVMConstInt(LLVMInt32TypeInContext(compiler->context), fieldInfo->index, false);

                //printf("left type = %s\n", LLVMPrintTypeToString(left.as.success.type));
                //printf("left value = %s\n", LLVMPrintValueToString(left.as.success.pointer));

                Estr ptrName = CREATE_ESTR(structInfo->name);
                APPEND_ESTR(ptrName, "_access");

                LLVMValueRef ptr = LLVMBuildGEP2(
                    compiler->builder,
                    left.as.success.type,
                    left.as.success.pointer,
                    (LLVMValueRef[]){ zero, index },
                    2,
                    ptrName.str
                );

                Estr valueName = CREATE_ESTR(fieldName);
                APPEND_ESTR(valueName, "_field");

                value = LLVMBuildLoad2(
                    compiler->builder,
                    fieldInfo->llvmType,
                    ptr,
                    valueName.str
                );

                isPointer = LLVMGetTypeKind(fieldInfo->llvmType) == LLVMStructTypeKind;

                type = Success(LLVMTypeRef, charptr, fieldInfo->llvmType);

                break;
            }

            default: {
                Estr errMsg = CREATE_ESTR("Unexpected operator for struct and ");
                APPEND_ESTR(errMsg, LLVMPrintTypeToString(right.as.success.type));
                APPEND_ESTR(errMsg, "!");

                return Error(CometValue, charptr, errMsg.str);
            }
        }
    } else {
        if (LLVMGetTypeKind(left.as.success.type) == LLVMIntegerTypeKind && LLVMGetTypeKind(right.as.success.type) == LLVMIntegerTypeKind) {

            if (right.error) return Error(CometValue, charptr, right.as.error);

            type = getType(compiler, "int");

            LLVMValuePair verified = verifyInts(compiler, left.as.success.value, right.as.success.value);
            left.as.success.value = verified.a;
            right.as.success.value = verified.b;

            switch (op.type) {
                case CT_PLUS: {
                    // we pass NULL to let LLVM decide the name of the SSA output
                    value = LLVMBuildAdd(compiler->builder, left.as.success.value, right.as.success.value, "");
                    break;
                }
                case CT_MINUS: {
                    value = LLVMBuildSub(compiler->builder, left.as.success.value, right.as.success.value, "");
                    break;
                }
                case CT_TIMES: {
                    value = LLVMBuildMul(compiler->builder, left.as.success.value, right.as.success.value, "");
                    break;
                }
                case CT_DIVIDE: {
                    value = LLVMBuildSDiv(compiler->builder, left.as.success.value, right.as.success.value, "");
                    break;
                }

                // conditionals
                case CT_EQ_EQ: {
                    type = getType(compiler, "bool");
                    value = LLVMBuildICmp(compiler->builder, LLVMIntEQ, left.as.success.value, right.as.success.value, "");
                    break;
                }
                case CT_LT: {
                    type = getType(compiler, "bool");
                    value = LLVMBuildICmp(compiler->builder, LLVMIntSLT, left.as.success.value, right.as.success.value, "");
                    break;
                }
                case CT_LTE: {
                    type = getType(compiler, "bool");
                    value = LLVMBuildICmp(compiler->builder, LLVMIntULE, left.as.success.value, right.as.success.value, "");
                    break;
                }
                case CT_GT: {
                    type = getType(compiler, "bool");
                    value = LLVMBuildICmp(compiler->builder, LLVMIntSGT, left.as.success.value, right.as.success.value, "");
                    break;
                }
                case CT_GTE: {
                    type = getType(compiler, "bool");
                    value = LLVMBuildICmp(compiler->builder, LLVMIntUGE, left.as.success.value, right.as.success.value, "");
                    break;
                }

                default:
                    return Error(CometValue, charptr, "Unexpected operator for int and int!");
            }
        } else {
            return Error(CometValue, charptr, "Cannot perform operation on those types.");
        }
    }

    if (type.error)
        return Error(CometValue, charptr, type.as.error);

    CometValue result;
    if (!isPointer) {
        result = (CometValue){
            .value = value,
            .type = type.as.success,
            .isPointer = false
        };
    } else {
        result = (CometValue){
            .pointer = value,
            .type = type.as.success,
            .isPointer = true
        };
    }

    return Success(CometValue, charptr, result);
}

ResultType(CometValue, charptr) visitFuncCall(CometCompiler* compiler, CometASTNode* node) {
    struct AST_FUNC_CALL funcCall = node->data.AST_FUNC_CALL;
    char* funcName = funcCall.ident->data.AST_IDENTIFIER.ident;

    Record* funcRecord = lookup(compiler->env, funcName);
    if (!funcRecord) {
        Estr errMsg = CREATE_ESTR("Attempt to call undefined function \"");
        APPEND_ESTR(errMsg, funcName);
        APPEND_ESTR(errMsg, "\"");

        return Error(CometValue, charptr, errMsg.str);
    }

    List(LLVMValueRef) argValues = newList(LLVMValueRef);
    for (size_t i = 0; i < funcCall.args.count; i++) {
        ResultType(CometValue, charptr) arg = resolveValue(compiler, *get(funcCall.args, i));
        if (arg.error)
            return arg;

        append(argValues, arg.as.success.value);
    }

    

    // ensure number of args passed to function is correct
    if (!LLVMIsFunctionVarArg(funcRecord->type)) {
        unsigned paramCount = LLVMCountParams(funcRecord->ptr);

        if (argValues.count < paramCount) {
            Estr errMsg = CREATE_ESTR("Not enough params passed to function \"");
            APPEND_ESTR(errMsg, funcName);
            APPEND_ESTR(errMsg, "\" (expects ");

            char* buffer = malloc(128);
            sprintf(buffer, "%d, passed %zu)", paramCount, argValues.count);
            APPEND_ESTR(errMsg, buffer);

            return Error(CometValue, charptr, errMsg.str);
        } else if (argValues.count > paramCount) {
            Estr errMsg = CREATE_ESTR("Too many params passed to function \"");
            APPEND_ESTR(errMsg, funcName);
            APPEND_ESTR(errMsg, "\" (expects ");

            char* buffer = malloc(128);
            sprintf(buffer, "%d, passed %zu)", paramCount, argValues.count);

            APPEND_ESTR(errMsg, buffer);
            return Error(CometValue, charptr, errMsg.str);
        }
    }

    LLVMValueRef returnValue = LLVMBuildCall2(compiler->builder, funcRecord->type, funcRecord->ptr, argValues.pointer, argValues.count, funcName);
    CometValue result = {
        .value = returnValue,
        .type = LLVMGetReturnType(funcRecord->type)
    };

    return Success(CometValue, charptr, result);
}


ResultType(CometValue, charptr) visitNewStatement(CometCompiler* compiler, CometASTNode* node) {
    struct AST_NEW_STATEMENT newStmt = node->data.AST_NEW_STATEMENT;
    char* structName = newStmt.structName->data.AST_IDENTIFIER.ident;

    ResultType(LLVMTypeRef, charptr) structType = getType(compiler, structName);
    if (structType.error)
        return Error(CometValue, charptr, structType.as.error);

    // get constructor
    Estr constructorName = CREATE_ESTR(structName);
    APPEND_ESTR(constructorName, "_CONSTRUCTOR");

    LLVMValueRef constructor = LLVMGetNamedFunction(compiler->module, constructorName.str);
    if (constructor == NULL) {
        Estr errMsg = CREATE_ESTR("Cannot use new on the struct \"");
        APPEND_ESTR(errMsg, structName);
        APPEND_ESTR(errMsg, "\" because it has no constructor.");
        return Error(CometValue, charptr, errMsg.str);
    }

    // get constructor info
    LLVMTypeRef constructorType = LLVMGlobalGetValueType(constructor);
    unsigned numParams = LLVMCountParamTypes(constructorType);
    LLVMTypeRef paramTypes[numParams];
    LLVMGetParamTypes(constructorType, paramTypes);

    List(LLVMValueRef) argValues = newList(LLVMValueRef);

    // create self
    LLVMValueRef self = LLVMBuildAlloca(compiler->builder, structType.as.success, "selfTmp");
    append(argValues, self);

    for (size_t i = 0; i < newStmt.args.count; i++) {
        ResultType(CometValue, charptr) arg = resolveValue(compiler, *get(newStmt.args, i));
        if (arg.error)
            return arg;

        append(argValues, arg.as.success.value);
    }

    // ensure number of args passed to constructor is correct
    if (newStmt.args.count < numParams-1) {
        Estr errMsg = CREATE_ESTR("Not enough params passed to constructor of struct \"");
        APPEND_ESTR(errMsg, structName);
        APPEND_ESTR(errMsg, "\" (expects ");

        char* buffer = malloc(128);
        sprintf(buffer, "%d, passed %zu)", numParams-1, newStmt.args.count);
        APPEND_ESTR(errMsg, buffer);

        return Error(CometValue, charptr, errMsg.str);
    } else if (newStmt.args.count > numParams-1) {
        Estr errMsg = CREATE_ESTR("Too many params passed to constructor of struct \"");
        APPEND_ESTR(errMsg, structName);
        APPEND_ESTR(errMsg, "\" (expects ");

        char* buffer = malloc(128);
        sprintf(buffer, "%d, passed %zu)", numParams-1, newStmt.args.count);

        APPEND_ESTR(errMsg, buffer);
        return Error(CometValue, charptr, errMsg.str);
    }

    LLVMBuildCall2(compiler->builder, constructorType, constructor, argValues.pointer, argValues.count, "");
    LLVMValueRef selfVal = LLVMBuildLoad2(compiler->builder, structType.as.success, self, "selfVal");
    
    CometValue result = {
        .value = selfVal,
        .type = structType.as.success,
        .isPointer = false
    };

    return Success(CometValue, charptr, result);
}
 
// -- MAIN --//
ResultType(Nothing, charptr) compileAST(CometCompiler* compiler, CometASTNode* root, const char* outputName) {
    ResultType(int, charptr) compilerResult = compile(compiler, root);
    if (compilerResult.error)
        return Error(Nothing, charptr, compilerResult.as.error);

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
    newCompiler->typeMap = newList(CometLLVMTypePair);

    LLVMTypeRef types[] = {
        LLVMIntTypeInContext(newCompiler->context, 8),   // small
        LLVMIntTypeInContext(newCompiler->context, 32),  // int
        LLVMIntTypeInContext(newCompiler->context, 64),  // big

        LLVMFloatTypeInContext(newCompiler->context),            // float
        LLVMDoubleTypeInContext(newCompiler->context),           // double

        LLVMIntTypeInContext(newCompiler->context, 8),  // bool

        LLVMVoidTypeInContext(newCompiler->context)             // void
    };
    
    for (size_t i = 0; i < sizeof(BUILT_IN_TYPES)/sizeof(BUILT_IN_TYPES[0]); i++) {
        CometLLVMTypePair new = {
            .typeName = (char*)BUILT_IN_TYPES[i],
            .llvmType = types[i]
        };

        append(newCompiler->typeMap, new);
    }

    // define structs list
    newCompiler->structs = newList(StructInfo);

    // set up env
    newCompiler->env = newEnvironment("root", NULL);

    // define internal constants
    defineInternalConstants(newCompiler);

    return Success(cometCompilerPtr, charptr, newCompiler);
}

ResultType(int, charptr) compile(CometCompiler* compiler, CometASTNode* node) {
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
        case AST_STRUCT_DEF_STATEMENT:
            return visitStructDefStatement(compiler, node);

        case AST_INFIX_EXPRESSION: {
            ResultType(CometValue, charptr) result = visitInfixExpression(compiler, node);
            if (result.error)
                return Error(int, charptr, result.as.error);

            return Success(int, charptr, false);
        }
        case AST_FUNC_CALL: {
            ResultType(CometValue, charptr) result = visitFuncCall(compiler, node);
            if (result.error)
                return Error(int, charptr, result.as.error);

            return Success(int, charptr, false);
        }

        default: {
            char* buffer = malloc(128);
            sprintf(buffer, "No visit method for %s node.", ASTNodeTypeToCStr(node->nodeType));
            return Error(int, charptr, buffer);
        }
    }

    return Success(int, charptr, false);
}