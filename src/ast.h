#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "lexer.h"


/*
Note:
    we're basically going for Pratt Parsing here, even though it's somewhat hard to implement in C
    we have statements and expressions and whatnot
*/



typedef enum {
    // literals
    AST_INT,
    AST_DOUBLE,
    AST_IDENTIFIER,
    AST_TYPE_NAME,

    AST_PROGRAM,

    // statements
    AST_EXPRESSION_STATEMENT,
    AST_ASSIGN_STATEMENT,
    AST_REASSIGN_STATEMENT,
    AST_WHILE_STATEMENT,
    AST_FOR_STATEMENT,

    // expressions
    AST_INFIX_EXPRESSION
} CometASTNodeType;

// Main ASTNode struct, this can hold the different types of nodes.
typedef struct CometASTNode CometASTNode;
struct CometASTNode {
    CometASTNodeType nodeType;
    union {
        struct AST_INT { int64_t number; } AST_INT;
        struct AST_DOUBLE { double number; } AST_DOUBLE;
        struct AST_IDENTIFIER { char* ident; } AST_IDENTIFIER;
        struct AST_TYPE_NAME { char* name; } AST_TYPE_NAME;

        struct AST_PROGRAM { CometASTNode** statements; size_t numStatements; size_t statementsArraySize; } AST_PROGRAM;

        struct AST_INFIX_EXPRESSION { CometASTNode* left; CometASTNode* right; char* op; } AST_INFIX_EXPRESSION;

        struct AST_EXPRESSION_STATEMENT { CometASTNode* expression; } AST_EXPRESSION_STATEMENT;
        struct AST_ASSIGN_STATEMENT { CometASTNode* ident; CometASTNode* expression; CometASTNode* type; } AST_ASSIGN_STATEMENT;
        struct AST_REASSIGN_STATEMENT { CometASTNode* ident; CometASTNode* expression; } AST_REASSIGN_STATEMENT;
        struct AST_WHILE_STATEMENT { CometASTNode* expression; CometASTNode* program; } AST_WHILE_STATEMENT;
        struct AST_FOR_STATEMENT { CometASTNode* ident; CometASTNode* start; CometASTNode* end; CometASTNode* step; CometASTNode* program; } AST_FOR_STATEMENT;
    } data;  
};

// put a node on the heap
CometASTNode* allocateNode(CometASTNode parent);
char* ASTNodeTypeToCStr(CometASTNodeType nodeType);

#define AST_NODE(nodeType, ...) allocateNode((CometASTNode){nodeType, {.nodeType=(struct nodeType){__VA_ARGS__}}})