#include "ast.h"

CometASTNode* allocateNode(CometASTNode node) {
    CometASTNode* ptr = malloc(sizeof(CometASTNode));
    if (ptr) *ptr = node;
    return ptr;
}

char* ASTNodeTypeToCStr(CometASTNodeType nodeType) {
    switch (nodeType) {
        case AST_INT:
            return "AST_INT";
        case AST_DOUBLE:
            return "AST_DOUBLE";
        case AST_STRING:
            return "AST_STRING";
        case AST_IDENTIFIER:
            return "AST_IDENTIFIER";
        case AST_BOOL:
            return "AST_BOOL";

        case AST_PROGRAM:
            return "AST_PROGRAM";

        case AST_EXPRESSION_STATEMENT:
            return "AST_EXPRESSION_STATEMENT";
        case AST_ASSIGN_STATEMENT:
            return "AST_ASSIGN_STATEMENT";
        case AST_REASSIGN_STATEMENT:
            return "AST_REASSIGN_STATEMENT";
        case AST_RETURN_STATEMENT:
            return "AST_RETURN_STATEMENT";
        case AST_IF_STATEMENT:
            return "AST_IF_STATEMENT";
        case AST_WHILE_STATEMENT:
            return "AST_WHILE_STATEMENT";
        case AST_FOR_STATEMENT:
            return "AST_FOR_STATEMENT";
        case AST_CONSTRUCTOR_DEF:
            return "AST_CONSTRUCTOR_DEF";

        case AST_INFIX_EXPRESSION:
            return "AST_INFIX_EXPRESSION";
        case AST_FUNC_CALL:
            return "AST_FUNC_CALL";
        case AST_NEW_STATEMENT:
            return "AST_NEW_STATEMENT";

        default:
            return "AST_UNKOWN (FIXME)";
    }
}