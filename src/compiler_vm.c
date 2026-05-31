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
CometType getType(CometCompiler* c, char* typeName) {
    for (size_t i = 0; i < c->typeMap.count; i++) {
        CometTypeMapEntry type = *get(c->typeMap, i);

        if (strcmp(type.name, typeName) == 0) {
            return type.type;
        }
    }

    return (CometType){
        .typeKind = COMET_VOID
    };
}

int32_t getStructIndex(CometCompiler* c, char* structName) {
    for (size_t i = 0; i < c->structs.count; i++) {
        CometStruct* structType = *get(c->structs, i);

        if (strcmp(structType->name, structName) == 0) {
            return i;
        }
    }

    return -1;
}

ResultType(CometOperand, charptr) visitProgram(CometCompiler* c, CometASTNode* p) {
    for (size_t i = 0; i < p->data.AST_PROGRAM.numStatements; i++) {
        ResultType(CometOperand, charptr) result = compile(c, p->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;
    }

    return Success(CometOperand, charptr, NO_OPERAND);
}

int rankType(CometType type) {
    switch (type.typeKind) {
        case COMET_BOOL: return 1;
        case COMET_SMALL: return 2;
        case COMET_INT: return 3;
        case COMET_BIG: return 4;
        case COMET_FLOAT: return 5;
        case COMET_DOUBLE: return 6;
        case COMET_STRUCT: return 7;
        default: return 0;
    }
}

CometType unifyType(CometType a, CometType b) {
    return (rankType(a) > rankType(b)) ? a : b;
}

ResultType(CometOperand, charptr) visitInfixExpression(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, charptr) visitFuncCall(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, charptr) visitNewStatement(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, charptr) visitValue(CometCompiler* c, CometASTNode* node) {
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

        case AST_NEW_STATEMENT: 
            return visitNewStatement(c, node);
        

        default: {
            Estr errMsg = CREATE_ESTR("Could not build expression: \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\"");

            return Error(CometOperand, charptr, errMsg.str);
        }
    }
}

ResultType(CometOperand, charptr) visitLVal(CometCompiler* c, CometASTNode* node) {
    return Success(CometOperand, charptr, NO_OPERAND);
}

ResultType(CometType, charptr) resolveType(CometCompiler* c, CometASTNode* node) {
    CometValueTypeKind outTypeKind;

    switch (node->nodeType) {
        case AST_INT: outTypeKind = COMET_INT; break;
        case AST_BOOL: outTypeKind = COMET_BOOL; break;
        case AST_DOUBLE: outTypeKind = COMET_DOUBLE; break;
        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(c->env, varName);

            if (!varRecord) {
                Estr errMsg = CREATE_ESTR("Undefined variable \"");
                APPEND_ESTR(errMsg, varName);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometType, charptr, errMsg.str);
            }

            return Success(CometType, charptr, varRecord->type);
        }
        case AST_NEW_STATEMENT: {
            CometType structType = (CometType){
                .typeKind = COMET_STRUCT,
                .structType = getType(c, node->data.AST_NEW_STATEMENT.structName->data.AST_IDENTIFIER.ident).structType
            };

            return Success(CometType, charptr, structType);
        }
        case AST_INFIX_EXPRESSION: {
            ResultType(CometType, charptr) left = resolveType(c, node->data.AST_INFIX_EXPRESSION.left);
            ResultType(CometType, charptr) right = resolveType(c, node->data.AST_INFIX_EXPRESSION.right);

            // division always results in a double
            if (node->data.AST_INFIX_EXPRESSION.op.type == CT_DIVIDE) {
                return Success(CometType, charptr, COMET_DOUBLE);
            }

            return Success(CometType, charptr, unifyType(left.as.success, right.as.success));
        }
        case AST_ARG_DEF: {
            CometType argType = getType(c, node->data.AST_ARG_DEF.type->data.AST_IDENTIFIER.ident);
            return Success(CometType, charptr, argType);
        }

        default: {
            Estr errMsg = CREATE_ESTR("Could not resolve type of expression: \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\"");

            return Error(CometType, charptr, errMsg.str);
        }
    }

    CometType outType = {
        .typeKind = outTypeKind
    };

    return Success(CometType, charptr, outType);
}

// -- VISIT METHODS -- //
ResultType(CometOperand, charptr) visitExpressionStatement(CometCompiler* c, CometASTNode* node) {
    return compile(c, node->data.AST_EXPRESSION_STATEMENT.expression);
}

ResultType(CometOperand, charptr) visitAssignStatement(CometCompiler* c, CometASTNode* node) {
    CometASTNode* expr = node->data.AST_ASSIGN_STATEMENT.expression;
    char* ident = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;

    if (!expr) {
        Estr errMsg = CREATE_ESTR("Variable \"");
        APPEND_ESTR(errMsg, ident);
        APPEND_ESTR(errMsg, "\" was not assigned a value.");
        return Error(CometOperand, charptr, errMsg.str);
    }

    Record* existingVar = lookup(c->env, ident);
    if (existingVar) {
        Estr errMsg = CREATE_ESTR("Redefinition of \"");
        APPEND_ESTR(errMsg, ident);
        APPEND_ESTR(errMsg, "\"")
        return Error(CometOperand, charptr, errMsg.str);
    }

    ResultType(CometType, charptr) exprType = resolveType(c, expr);
    if (exprType.error)
        return Error(CometOperand, charptr, exprType.as.error);

    ResultType(CometOperand, charptr) exprResult = visitValue(c, expr);
    if (exprResult.error)
        return exprResult;

    uint32_t idx = defineVar(c->env, ident, RECORD_LOCAL, exprResult.as.success, exprType.as.success, node->data.AST_ASSIGN_STATEMENT.isMutable);
    buildStore(c, idx);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitFieldReassignStatement(CometCompiler* c, CometASTNode* field, CometOperand newValue) {
    ResultType(CometOperand, charptr) fieldPtr = visitValue(c, field);
    if (fieldPtr.error)
        return fieldPtr;

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitReassignStatement(CometCompiler* c, CometASTNode* node) {
    ResultType(CometOperand, charptr) exprResult = visitValue(c, node->data.AST_ASSIGN_STATEMENT.expression);
    if (exprResult.error)
        return exprResult;

    if (node->data.AST_ASSIGN_STATEMENT.ident->nodeType == AST_INFIX_EXPRESSION) { // struct reassign
        return visitFieldReassignStatement(c, node->data.AST_REASSIGN_STATEMENT.ident, exprResult.as.success);
    }
    
    char* ident = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;

    Record* varRecord = lookup(c->env, ident);
    if (!varRecord) {
        Estr errMsg = CREATE_ESTR("Cannot reassign undefined variable \"");
        APPEND_ESTR(errMsg, ident);
        APPEND_ESTR(errMsg, "\"");
        return Error(CometOperand, charptr, errMsg.str);
    }

    if (!varRecord->isMutable) {
        Estr errMsg = CREATE_ESTR("Cannot change value of immutable variable \"");
        APPEND_ESTR(errMsg, ident);
        APPEND_ESTR(errMsg, "\", did you forget \"mut\"?");
        return Error(CometOperand, charptr, errMsg.str);
    }

    buildStore(c, varRecord->recordIdx);
    return Success(CometOperand, charptr, NO_OPERAND);
}

ResultType(CometOperand, charptr) getField(CometCompiler* c, CometASTNode* structToGet, CometASTNode* field) {
    char* fieldName = field->data.AST_IDENTIFIER.ident;

    ResultType(CometType, charptr) structType = resolveType(c, structToGet);
    if (structType.error)
        return Error(CometOperand, charptr, structType.as.error);

    ResultType(CometOperand, charptr) structValue = visitValue(c, structToGet);
    if (structValue.error) 
        return structValue;

    CometOperand dest = buildGetField(c, getFieldIndex(structType.as.success.structType, fieldName));

    return Success(CometOperand, charptr, dest);
}
ResultType(CometOperand, charptr) visitInfixExpression(CometCompiler* c, CometASTNode* node) {
    struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

    ResultType(CometType, charptr) leftType = resolveType(c, expr.left);
    if (leftType.error)
        return Error(CometOperand, charptr, leftType.as.error);
    ResultType(CometType, charptr) rightType = resolveType(c, expr.right);
    if (rightType.error)
        return Error(CometOperand, charptr, rightType.as.error);

    if (expr.op.type == CT_DOT) { // getting a field
        return getField(c, expr.left, expr.right);
    }

    CometType resultType = unifyType(leftType.as.success, rightType.as.success);
    
    visitValue(c, expr.left);
    if (typesAreEqual(leftType.as.success, resultType)) {
        leftType.as.success = buildCast(c, leftType.as.success, resultType);
    }
    visitValue(c, expr.right);
    if (typesAreEqual(rightType.as.success, resultType)) {
        rightType.as.success = buildCast(c, rightType.as.success, resultType);
    }

    CometOperand out;

    // int operations
    switch (expr.op.type) {
        // arithmetic
        case CT_PLUS: {
            out = buildAdd(c, resultType);
            break;
        }
        case CT_MINUS: {
            out = buildSub(c, resultType);
            break;
        }
        case CT_TIMES: {
            out = buildMul(c, resultType);
            break;
        }
        case CT_DIVIDE: {
            out = buildDiv(c, resultType);
            break;
        }

        // conditionals
        case CT_EQ_EQ: {
            out = buildEq(c, resultType);
            break;
        }
        case CT_NOT_EQ: {
            out = buildNeq(c, resultType);
            break;
        }
        case CT_LT: {
            out = buildLt(c, resultType);
            break;
        }
        case CT_GT: {
            out = buildGt(c, resultType);
            break;
        }
        case CT_LTE: {
            out = buildLte(c, resultType);
            break;
        }
        case CT_GTE: {
            out = buildGte(c, resultType);
            break;
        }

        default: {
            Estr errMsg = CREATE_ESTR("Invalid operator for int and int: \"");
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
    CometType funcType = {
        .typeKind = COMET_FUNCTION,
        .functionType = getValueType(c, funcValue).functionType
    };
    defineVar(c->env, funcName, RECORD_LOCAL, funcValue, funcType, false);

    // create the new scope for the function
    CometEnvironment* funcEnv = newEnvironment(funcName, c->env);
    c->env = funcEnv;

    // define each argument in the functions scope
    for (size_t argIdx = 0; argIdx < funcDef.args.count; argIdx++) {
        CometASTNode* argNode = *get(funcDef.args, argIdx);
        struct AST_ARG_DEF argument = argNode->data.AST_ARG_DEF;

        ResultType(CometType, charptr) argType = resolveType(c, argNode);
        if (argType.error)
            return Error(CometOperand, charptr, argType.as.error);

        CometOperand argValue = createOperand(CO_IMMEDIATE);
        argValue.imm.typeKind = COMET_SMALL;
        argValue.imm.smallVal = argIdx;

        defineVar(
            c->env,
            argument.ident->data.AST_IDENTIFIER.ident, 
            RECORD_ARG,
            argValue, 
            argType.as.success,
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
    ResultType(CometOperand, charptr) returnValue = visitValue(c, node->data.AST_RETURN_STATEMENT.expression);
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

        ResultType(CometOperand, charptr) argValue = visitValue(c, argNode);
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

    ResultType(CometOperand, charptr) condition = visitValue(c, ifStmt.expression);
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
        condition = visitValue(c, ifStmt.expression);
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
ResultType(CometOperand, charptr) visitWhileStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_WHILE_STATEMENT whileStmt = node->data.AST_WHILE_STATEMENT;
    
    CometLabel* startLabel = buildLabel(c);
    CometLabel* endLabel = buildLabel(c);

    

    ResultType(CometOperand, charptr) condition = visitValue(c, whileStmt.expression);
    if (condition.error)
        return condition;

    buildJumpIfFalse(c, endLabel);
    resolveLabel(c, startLabel);

    ResultType(CometOperand, charptr) whileBodyResult = compile(c, whileStmt.program);
    if (whileBodyResult.error)
        return whileBodyResult;

    condition = visitValue(c, whileStmt.expression);
    if (condition.error)
        return condition;

    buildNot(c);
    buildJumpIfFalse(c, startLabel);

    resolveLabel(c, endLabel);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitForStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_FOR_STATEMENT forStmt = node->data.AST_FOR_STATEMENT;

    CometLabel* mainLabel = buildLabel(c);
    CometLabel* endLabel = buildLabel(c);

    // resolve start and end types
    ResultType(CometType, charptr) startType = resolveType(c, forStmt.start);
    if (startType.error)
        return Error(CometOperand, charptr, startType.as.error);
    ResultType(CometType, charptr) endType = resolveType(c, forStmt.end);
    if (endType.error)
        return Error(CometOperand, charptr, endType.as.error);
    CometType resultType = unifyType(startType.as.success, endType.as.success);

    char* ident = forStmt.ident->data.AST_IDENTIFIER.ident;

    // create env for for loop
    CometEnvironment* forLoopEnv = newEnvironment("", c->env);
    CometEnvironment* previousEnv = c->env;
    c->env = forLoopEnv;

    // define iterator variable
    ResultType(CometOperand, charptr) start = visitValue(c, forStmt.start);
    if (start.error)
        return Error(CometOperand, charptr, start.as.error);

    uint32_t idx = defineVar(c->env, ident, RECORD_LOCAL, start.as.success, startType.as.success, false);
    buildStore(c, idx);

    resolveLabel(c, mainLabel);

    // if the iterator var is equal to the end value, then we jump to the exit of the for loop
    buildLoad(c, idx);

    ResultType(CometOperand, charptr) end  = visitValue(c, forStmt.end);
    if (endType.error)
        return Error(CometOperand, charptr, endType.as.error);

    buildNeq(c, startType.as.success);
    buildJumpIfFalse(c, endLabel);

    // compile the body of the for loop
    ResultType(CometOperand, charptr) bodyResult = compile(c, forStmt.program);
    if (bodyResult.error)
        return bodyResult;

    // compile the step value
    ResultType(CometType, charptr) stepType = resolveType(c, forStmt.step);
    if (stepType.error)
        return Error(CometOperand, charptr, stepType.as.error);

    buildLoad(c, idx);

    ResultType(CometOperand, charptr) step = visitValue(c, forStmt.step);
    if (step.error)
        return Error(CometOperand, charptr, step.as.error);

    CometType addType = unifyType(startType.as.success, stepType.as.success);

    // add the step to the iterator var
    buildAdd(c, addType);
    buildDup(c);

    // save the iterator value
    buildStore(c, idx);

    // jump back to the start of the for loop
    buildJump(c, mainLabel);

    // resolve label for end of loop
    resolveLabel(c, endLabel);

    // exit the for loop's env
    c->env = previousEnv;
    free(forLoopEnv);

    

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitConstructorDefStatement(CometCompiler* c, CometASTNode* node, char* constructorName, CometType structType) {
    struct AST_CONSTRUCTOR_DEF constDef = node->data.AST_CONSTRUCTOR_DEF;

    buildFunction(c, constructorName, constDef.args.count + 1); // add 1 arg for self

    // create the new scope for the function
    CometEnvironment* funcEnv = newEnvironment("", c->env);
    c->env = funcEnv;

    // define self
    CometOperand selfValue = createOperand(CO_IMMEDIATE);
    selfValue.imm.typeKind = COMET_SMALL;
    selfValue.imm.smallVal = 0;

    uint32_t selfIdx = defineVar(
        c->env,
        "self",
        RECORD_ARG,
        selfValue,
        structType,
        false
    );

    // define each argument in the functions scope
    for (size_t argIdx = 0; argIdx < constDef.args.count; argIdx++) {
        CometASTNode* argNode = *get(constDef.args, argIdx);
        struct AST_ARG_DEF argument = argNode->data.AST_ARG_DEF;

        ResultType(CometType, charptr) argType = resolveType(c, argNode);
        if (argType.error)
            return Error(CometOperand, charptr, argType.as.error);

        CometOperand argValue = createOperand(CO_IMMEDIATE);
        argValue.imm.typeKind = COMET_SMALL;
        argValue.imm.smallVal = argIdx + 1; // add 1 for self

        defineVar(
            c->env,
            argument.ident->data.AST_IDENTIFIER.ident, 
            RECORD_ARG,
            argValue, 
            argType.as.success,
            false
        );
    }

    // build the functions body
    ResultType(CometOperand, charptr) bodyResult = compile(c, constDef.program);
    if (bodyResult.error)
        return bodyResult;

    // build return
    buildLoadArg(c, selfIdx);
    buildReturn(c);

    // return back to the parent scope
    c->env = c->env->parent;
    free(funcEnv);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitStructDefStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_STRUCT_DEF_STATEMENT structDef = node->data.AST_STRUCT_DEF_STATEMENT;

    CometStruct* structType = malloc(sizeof(CometStruct));
    char* structName = structDef.ident->data.AST_IDENTIFIER.ident;

    structType->name = structName;

    // define fields
    size_t fieldCount = 0;
    size_t methodCount = 0;
    for (size_t i = 0; i < structDef.fieldDefs.count; i++) {
        CometASTNode* fieldDef = *get(structDef.fieldDefs, i);

        switch (fieldDef->nodeType) {
            case AST_ASSIGN_STATEMENT:
                fieldCount++;
                break;

            case AST_FUNC_DEF_STATEMENT:
                methodCount++;
                break;

            default: {
                Estr errMsg = CREATE_ESTR("Cannot define \"");
                APPEND_ESTR(errMsg, ASTNodeTypeToCStr(fieldDef->nodeType));
                APPEND_ESTR(errMsg, "\" in struct.");

                return Error(CometOperand, charptr, errMsg.str);
            }
        }
    }
    structType->fieldCount = fieldCount;
    structType->numMethods = methodCount;
    structType->fieldNames = calloc(structType->fieldCount, sizeof(char*));
    structType->vtable = calloc(structType->numMethods, sizeof(CometMethod*));

    for (size_t i = 0; i < structDef.fieldDefs.count; i++) {
        CometASTNode* fieldDef = *get(structDef.fieldDefs, i);

        switch (fieldDef->nodeType) {
            case AST_ASSIGN_STATEMENT:
                structType->fieldNames[i] = fieldDef->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;
                break;

            default: break;
        }
    }

    // build constructor
    if (!structDef.constructor) {
        Estr errMsg = CREATE_ESTR("Struct \"");
        APPEND_ESTR(errMsg, structName);
        APPEND_ESTR(errMsg, "is missing a constructor!");

        return Error(CometOperand, charptr, errMsg.str);
    }
    Estr constructorName = CREATE_ESTR(strdup(structName));
    APPEND_ESTR(constructorName, "_INIT");

    CometType generalStructType = {
        .typeKind = COMET_STRUCT,
        .structType = structType
    };

    ResultType(CometOperand, charptr) constructorResult = visitConstructorDefStatement(c, structDef.constructor, constructorName.str, generalStructType);
    if (constructorResult.error)
        return constructorResult;

    CometTypeMapEntry typeMapEntry = {
        .name = strdup(structName),
        .type = {
            .typeKind = COMET_STRUCT,
            .structType = structType
        }
    };

    append(c->typeMap, typeMapEntry);
    append(c->structs, structType);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitNewStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_NEW_STATEMENT newStmt = node->data.AST_NEW_STATEMENT;

    // get struct type
    char* structName = newStmt.structName->data.AST_IDENTIFIER.ident;
    int32_t idx = getStructIndex(c, structName);

    if (idx == -1) {
        Estr errMsg = CREATE_ESTR("The type \"");
        APPEND_ESTR(errMsg, structName);
        APPEND_ESTR(errMsg, "\" was not found.");

        return Error(CometOperand, charptr, errMsg.str);
    }

    // make new instance
    buildNew(c, idx);

    // push args for constructor
    List(CometOperand) funcCallArgs = newList(CometOperand);
    for (size_t argIdx = 0; argIdx < newStmt.args.count; argIdx++) {
        CometASTNode* argNode = *get(newStmt.args, argIdx);

        ResultType(CometOperand, charptr) argValue = visitValue(c, argNode);
        if (argValue.error)
            return argValue;

        append(funcCallArgs, argValue.as.success);
    }

    // call constructor
    Estr constructorName = CREATE_ESTR(structName);
    APPEND_ESTR(constructorName, "_INIT");
    buildCall(c, constructorName.str, funcCallArgs);
    DESTROY_ESTR(constructorName);

    // return
    return Success(CometOperand, charptr, NO_OPERAND);
}

// -- MAIN -- //
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
        .numFunctions = c->functionCount,
        .numStructs = c->structs.count
    };

    fwrite(&cometFile, sizeof(CometFile), 1, file);
    fwrite(c->consts, sizeof(CometOperand), c->constIdx, file);


    for (size_t i = 0; i < c->functionCount; i++) {
        CometSerializedFunc serializedFunc = {
            .startIdx = c->functions[i]->startIdx,
            .numArgs = c->functions[i]->argCount
        };
        strcpy(serializedFunc.name, c->functions[i]->name);

        fwrite(&serializedFunc, sizeof(CometSerializedFunc), 1, file);
        
    }

    for (size_t structIdx = 0; structIdx < c->structs.count; structIdx++) {
        CometStruct* structType = *get(c->structs, structIdx);
        CometSerializedStruct* serializedStruct = serializeStruct(c, structType);

        CometSerializedStructHeader header = {
            .numFields = serializedStruct->numFields,
            .numMethods = serializedStruct->numMethods
        };

        fwrite(&header, sizeof(CometSerializedStructHeader), 1, file);
        fwrite(serializedStruct->vtable, sizeof(uint32_t), serializedStruct->numMethods, file);
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
        case AST_REASSIGN_STATEMENT:
            return visitReassignStatement(c, node);
        case AST_FUNC_DEF_STATEMENT:
            return visitFuncDefStatement(c, node);
        case AST_RETURN_STATEMENT:
            return visitReturnStatement(c, node);
        case AST_IF_STATEMENT:
            return visitIfStatement(c, node);
        case AST_WHILE_STATEMENT:
            return visitWhileStatement(c, node);
        case AST_FOR_STATEMENT:
            return visitForStatement(c, node);
        case AST_STRUCT_DEF_STATEMENT:
            return visitStructDefStatement(c, node);
        case AST_NEW_STATEMENT:
            return visitNewStatement(c, node);
        
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