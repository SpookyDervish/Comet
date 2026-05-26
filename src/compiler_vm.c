#include "compiler_vm.h"
#include "ast.h"
#include "environment.h"
#include "inst.h"
#include "lexer.h"
#include "operand.h"
#include "parser.h"
#include "token.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>


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
ResultType(CometOperand, charptr) resolveValue(CometCompiler* c, CometASTNode* node) {
    switch (node->nodeType) {
        case AST_INFIX_EXPRESSION:
            return visitInfixExpression(c, node);

        case AST_INT: {
            CometOperand new = createOperand(CO_IMMEDIATE);
            new.imm.typeKind = COMET_INT;
            new.imm.intVal = node->data.AST_INT.number;

            CometOperand idx = storeConst(c, new);
            buildPushConst(c, idx);

            return Success(CometOperand, charptr, new);
        }

        case AST_IDENTIFIER: {
            uint32_t idx = lookup(c->env, node->data.AST_IDENTIFIER.ident)->recordIdx;

            buildLoad(c, idx);
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
    ResultType(CometOperand, charptr) exprResult = compile(c, node->data.AST_ASSIGN_STATEMENT.expression);
    char* ident = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;

    uint32_t idx = defineVar(c->env, ident, exprResult.as.success, node->data.AST_ASSIGN_STATEMENT.isMutable);
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

        default: {
            Estr errMsg = CREATE_ESTR("Invalid operator \"");
            APPEND_ESTR(errMsg, tokenTypeToCStr(expr.op.type));
            APPEND_ESTR(errMsg, "\"");

            return Error(CometOperand, charptr, errMsg.str);
        }
    }

    return Success(CometOperand, charptr, out);
}

// -- MAIN -- //
CometCompiler* createCompilerVM() {
    CometCompiler* newCompiler = calloc(sizeof(CometCompiler), 1);
    return newCompiler;
}

ResultType(CometOperand, charptr) compile(CometCompiler* c, CometASTNode* node) {
    
    printf("%s\n", ASTNodeTypeToCStr(node->nodeType));
    switch (node->nodeType) {
        case AST_PROGRAM:
            return visitProgram(c, node);

        case AST_EXPRESSION_STATEMENT:
            return visitExpressionStatement(c, node);
        case AST_ASSIGN_STATEMENT:
            return visitAssignStatement(c, node);

        case AST_INFIX_EXPRESSION: {
            ResultType(CometOperand, charptr) value = visitInfixExpression(c, node);
            return value;
        }

        default: {
            Estr errMsg = CREATE_ESTR("No compiler visit method for \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\"!");

            return Error(CometOperand, charptr, errMsg.str);
        }
    }
}