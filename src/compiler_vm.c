#include "compiler_vm.h"
#include "ast.h"
#include "lexer.h"
#include <stdlib.h>


// -- HELPER METHODS -- //
CometOperand createOperand(CometOperandKind type) {
    return (CometOperand){
        .type = type
    };
}

ResultType(CometOperand, charptr) visitInfixExpression(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, charptr) resolveValue(CometCompiler* c, CometASTNode* node) {
    switch (node->nodeType) {
        case AST_INFIX_EXPRESSION:
            return visitInfixExpression(c, node);

        case AST_INT:
            CometOperand new = createOperand(CO_CONST);
            
    }
}

// -- VISIT METHODS -- //
ResultType(voidPtr, charptr) visitExpressionStatement(CometCompiler* c, CometASTNode* node) {
    return compile(c, node->data.AST_EXPRESSION_STATEMENT.expression);
}

ResultType(CometOperand, charptr) visitInfixExpression(CometCompiler* c, CometASTNode* node) {
    ResultType(CometOperand, charptr) left = resolveValue(c, node);
}

// -- MAIN -- //
CometCompiler* createCompilerVM() {
    CometCompiler* newCompiler = calloc(sizeof(CometCompiler), 1);
    return newCompiler;
}

ResultType(voidPtr, charptr) compile(CometCompiler* c, CometASTNode* node) {
    switch (node->nodeType) {
        case AST_EXPRESSION_STATEMENT:
            return visitExpressionStatement(c, node);

        case AST_INFIX_EXPRESSION:
            ResultType(CometOperand, charptr) value = visitInfixExpression(c, node);
            if (value.error) {
                return Error(voidPtr, charptr, value.as.error);
            }

        default: {
            Estr errMsg = CREATE_ESTR("No compiler visit method for \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\"!");

            return Error(voidPtr, charptr, errMsg.str);
        }
    }
}