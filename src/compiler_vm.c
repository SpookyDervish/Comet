#include "compiler_vm.h"
#include "ast.h"
#include "environment.h"
#include "inst.h"
#include "lexer.h"
#include "../include/operand.h"
#include "serialize.h"
#include "token.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


// -- HELPER METHODS -- //
ResultType(CometOperand, charptr) visitProgram(CometCompiler* c, CometASTNode* p) {
    for (size_t i = 0; i < p->data.AST_PROGRAM.numStatements; i++) {
        ResultType(CometOperand, charptr) result = compile(c, p->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;
    }

    return Success(CometOperand, charptr, NO_OPERAND);
}

ResultType(CometOperand, charptr) visitInfixExpression(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, charptr) visitFuncCall(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, charptr) resolveValue(CometCompiler* c, CometASTNode* node) {
    switch (node->nodeType) {
        case AST_INFIX_EXPRESSION:
            return visitInfixExpression(c, node);

        case AST_FUNC_CALL:
            return visitFuncCall(c, node);

        case AST_INT: {
            CometOperand new = createOperand(CO_IMMEDIATE);
            new.imm.typeKind = COMET_INT;
            new.imm.intVal = node->data.AST_INT.number;

            CometOperand idx = storeConst(c, new);
            buildPushConst(c, idx);

            return Success(CometOperand, charptr, new);
        }

        case AST_BOOL: {
            CometOperand new = createOperand(CO_IMMEDIATE);
            new.imm.typeKind = COMET_BOOL;
            new.imm.boolVal = node->data.AST_BOOL.value;

            CometOperand idx = storeConst(c, new);
            buildPushConst(c, idx);

            return Success(CometOperand, charptr, new);
        }

        case AST_DOUBLE: {
            CometOperand new = createOperand(CO_IMMEDIATE);
            new.imm.typeKind = COMET_DOUBLE;
            new.imm.doubleVal = node->data.AST_DOUBLE.number;

            CometOperand idx = storeConst(c, new);
            buildPushConst(c, idx);

            return Success(CometOperand, charptr, new);
        }

        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(c->env, varName);

            if (varRecord == NULL) {
                Estr errMsg = CREATE_ESTR("Undefined variable \"");
                APPEND_ESTR(errMsg, varName);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometOperand, charptr, errMsg.str);
            }

            uint32_t idx = varRecord->recordIdx;

            CometOperand new;
            switch (varRecord->recordType) {
                case RECORD_LOCAL: 
                   new = buildLoad(c, idx);
                   break;
                case RECORD_ARG:
                    new = buildLoadArg(c, idx);
                    break;
                
            }
             
            return Success(CometOperand, charptr, new);
            
        }

        default: {
            Estr errMsg = CREATE_ESTR("Could not resolve type of expression: \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\"");

            return Error(CometOperand, charptr, errMsg.str);
        }
    }
}

// -- VISIT METHODS -- //
ResultType(CometOperand, charptr) visitExpressionStatement(CometCompiler* c, CometASTNode* node) {
    return compile(c, node->data.AST_EXPRESSION_STATEMENT.expression);
}

ResultType(CometOperand, charptr) visitAssignStatement(CometCompiler* c, CometASTNode* node) {
    ResultType(CometOperand, charptr) exprResult = resolveValue(c, node->data.AST_ASSIGN_STATEMENT.expression);
    if (exprResult.error)
        return exprResult;
    
    char* ident = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;

    uint32_t idx = defineVar(c->env, ident, RECORD_LOCAL, exprResult.as.success, node->data.AST_ASSIGN_STATEMENT.isMutable);
    buildStore(c, idx);

    return Success(CometOperand, charptr, NO_OPERAND);
}

ResultType(CometOperand, charptr) visitInfixExpression(CometCompiler* c, CometASTNode* node) {
    struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

    ResultType(CometOperand, charptr) left = resolveValue(c, expr.left);
    if (left.error)
        return left;
    ResultType(CometOperand, charptr) right = resolveValue(c, expr.right);
    if (right.error)
        return right;
    
    CometOperand out;
    switch (expr.op.type) {
        // arithmetic
        case CT_PLUS: {
            out = buildAdd(c);
            break;
        }
        case CT_MINUS: {
            out = buildSub(c);
            break;
        }
        case CT_TIMES: {
            out = buildMul(c);
            break;
        }

        // conditionals
        case CT_EQ_EQ: {
            out = buildEq(c);
            break;
        }

        default: {
            Estr errMsg = CREATE_ESTR("Invalid operator \"");
            APPEND_ESTR(errMsg, tokenTypeToCStr(expr.op.type));
            APPEND_ESTR(errMsg, "\"");

            return Error(CometOperand, charptr, errMsg.str);
        }
    }

    return Success(CometOperand, charptr, out);
}
ResultType(CometOperand, charptr) visitFuncDefStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_FUNC_DEF_STATEMENT funcDef = node->data.AST_FUNC_DEF_STATEMENT;
    char* funcName = funcDef.ident->data.AST_IDENTIFIER.ident;

    
    // build the function start and define the function in the current scope
    CometOperand funcValue = buildFunction(c, funcName, funcDef.args.count);
    defineVar(c->env, funcName, RECORD_LOCAL, funcValue, false);

    // create the new scope for the function
    CometEnvironment* funcEnv = newEnvironment(funcName, c->env);
    c->env = funcEnv;

    // define each argument in the functions scope
    for (size_t argIdx = 0; argIdx < funcDef.args.count; argIdx++) {
        CometASTNode* argNode = *get(funcDef.args, argIdx);
        struct AST_ARG_DEF argument = argNode->data.AST_ARG_DEF;

        CometOperand argValue = createOperand(CO_IMMEDIATE);
        argValue.imm.typeKind = COMET_INT;
        argValue.imm.intVal = argIdx;

        defineVar(
            c->env,
            argument.ident->data.AST_IDENTIFIER.ident, 
            RECORD_ARG,
            argValue, 
            false
        );
    }

    // build the functions body
    ResultType(CometOperand, charptr) bodyResult = compile(c, funcDef.program);
    if (bodyResult.error)
        return bodyResult;

    // return back to the parent scope
    c->env = c->env->parent;
    free(funcEnv);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitReturnStatement(CometCompiler* c, CometASTNode* node) {
    ResultType(CometOperand, charptr) returnValue = resolveValue(c, node->data.AST_RETURN_STATEMENT.expression);
    if (returnValue.error)
        return returnValue;
    
    buildReturn(c);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitFuncCall(CometCompiler* c, CometASTNode* node) {
    struct AST_FUNC_CALL funcCall = node->data.AST_FUNC_CALL;
    char* funcName = funcCall.ident->data.AST_IDENTIFIER.ident;

    List(CometOperand) funcCallArgs = newList(CometOperand);
    for (size_t argIdx = 0; argIdx < funcCall.args.count; argIdx++) {
        CometASTNode* argNode = *get(funcCall.args, argIdx);

        ResultType(CometOperand, charptr) argValue = resolveValue(c, argNode);
        if (argValue.error)
            return argValue;

        append(funcCallArgs, argValue.as.success);
    }

    CometOperand returnValue = buildCall(c, funcName, funcCallArgs);
    return Success(CometOperand, charptr, returnValue);
}
ResultType(CometOperand, charptr) visitIfStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_IF_STATEMENT ifStmt = node->data.AST_IF_STATEMENT;
    CometASTNode* elseBody = ifStmt.elseProgram;

    CometLabel* endLabel = buildLabel(c);
    CometLabel* elseLabel = buildLabel(c);

    ResultType(CometOperand, charptr) condition = resolveValue(c, ifStmt.expression);
    if (condition.error)
        return condition;

    if (elseBody != NULL)
        buildJumpIfFalse(c, elseLabel);
    else
        buildJumpIfFalse(c, endLabel);

    ResultType(CometOperand, charptr) ifBodyResult = compile(c, ifStmt.program);
    if (ifBodyResult.error)
        return ifBodyResult;

    

    

    if (ifStmt.elseProgram != NULL) {
        buildJump(c, endLabel);
        resolveLabel(c, elseLabel);
        condition = resolveValue(c, ifStmt.expression);
        if (condition.error)
            return condition;

        buildNot(c);
        buildJumpIfFalse(c, endLabel);

        ResultType(CometOperand, charptr) elseBodyResult = compile(c, ifStmt.elseProgram);
        if (elseBodyResult.error)
            return elseBodyResult;

    }

    resolveLabel(c, endLabel);
    

    return Success(CometOperand, charptr, NO_OPERAND);
}

// -- MAIN -- //
CometCompiler* createCompilerVM() {
    CometCompiler* newCompiler = calloc(1, sizeof(CometCompiler));
    return newCompiler;
}

ResultType(voidPtr, charptr) outputToFile(CometCompiler* c, const char* filePath) {
    FILE* file = fopen(filePath, "wb");
    if (file == NULL) {
        return Error(voidPtr, charptr, strerror(errno));
    }

    CometFile cometFile = {
        .magic = {'C', 'O', 'M', 'E',  'T'},
        .version = 1,
        .numConsts = c->constIdx,
        .numInstructions = c->programIdx,
        .numFunctions = c->functionCount
    };

    fwrite(&cometFile, sizeof(CometFile), 1, file);
    fwrite(c->consts, sizeof(CometOperand), c->constIdx, file);


    for (size_t i = 0; i < c->functionCount; i++) {
        CometSerializedFunc serializedFunc = {
            .startIdx = c->functions[i]->startIdx
        };
        strcpy(serializedFunc.name, c->functions[i]->name);

        fwrite(&serializedFunc, sizeof(CometSerializedFunc), 1, file);
        
    }

    for (size_t instIdx = 0; instIdx < c->programIdx; instIdx++) {
        CometInst inst = c->outputProgram[instIdx];
        CometSerializedInst* serializedInst = serializeInst(c, inst);

        fwrite(serializedInst, sizeof(CometSerializedInst), 1, file);
        
    }
    fclose(file);

    return Success(voidPtr, charptr, NULL);
}

ResultType(CometOperand, charptr) compile(CometCompiler* c, CometASTNode* node) {
    
    switch (node->nodeType) {
        case AST_PROGRAM:
            return visitProgram(c, node);

        case AST_EXPRESSION_STATEMENT:
            return visitExpressionStatement(c, node);
        case AST_ASSIGN_STATEMENT:
            return visitAssignStatement(c, node);
        case AST_FUNC_DEF_STATEMENT:
            return visitFuncDefStatement(c, node);
        case AST_RETURN_STATEMENT:
            return visitReturnStatement(c, node);
        case AST_IF_STATEMENT:
            return visitIfStatement(c, node);
        
        case AST_FUNC_CALL:
            return visitFuncCall(c, node);
        case AST_INFIX_EXPRESSION: 
            return visitInfixExpression(c, node);
        
        default: {
            Estr errMsg = CREATE_ESTR("No compiler visit method for \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\"!");

            return Error(CometOperand, charptr, errMsg.str);
        }
    }
}