#include "compiler_vm.h"
#include "ast.h"
#include "inst.h"
#include "lexer.h"
#include "token.h"
#include <stddef.h>
#include <stdlib.h>


// -- HELPER METHODS -- //
ResultType(voidPtr, charptr) visitProgram(CometCompiler* c, CometASTNode* p) {
    for (size_t i = 0; i < p->data.AST_PROGRAM.numStatements; i++) {
        ResultType(voidPtr, charptr) result = compile(c, p->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;
    }

    return Success(voidPtr, charptr, NULL);
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

        default: {
            Estr errMsg = CREATE_ESTR("Could not resolve type of expression: \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\"");

            return Error(CometOperand, charptr, errMsg.str);
        }
    }
}

// -- VISIT METHODS -- //
ResultType(voidPtr, charptr) visitExpressionStatement(CometCompiler* c, CometASTNode* node) {
    return compile(c, node->data.AST_EXPRESSION_STATEMENT.expression);
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

ResultType(voidPtr, charptr) compile(CometCompiler* c, CometASTNode* node) {
    switch (node->nodeType) {
        case AST_PROGRAM:
            return visitProgram(c, node);

        case AST_EXPRESSION_STATEMENT:
            return visitExpressionStatement(c, node);

        case AST_INFIX_EXPRESSION: {
            ResultType(CometOperand, charptr) value = visitInfixExpression(c, node);
            if (value.error) {
                return Error(voidPtr, charptr, value.as.error);
            }
            return Success(voidPtr, charptr, NULL);
        }

        default: {
            Estr errMsg = CREATE_ESTR("No compiler visit method for \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\"!");

            return Error(voidPtr, charptr, errMsg.str);
        }
    }
}