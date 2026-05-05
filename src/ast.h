#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "lexer.h"
#include "token.h"


/*
Note:
    we're basically going for Pratt Parsing here, even though it's somewhat hard to implement in C
    we have statements and expressions and whatnot
*/



typedef enum {
    // literals
    AST_INT,
    AST_DOUBLE,
    AST_STRING,
    AST_IDENTIFIER,
    AST_FUNC_CALL,

    AST_ARG_DEF,

    AST_PROGRAM,

    // statements
    AST_EXPRESSION_STATEMENT,
    AST_ASSIGN_STATEMENT,
    AST_REASSIGN_STATEMENT,
    AST_WHILE_STATEMENT,
    AST_FOR_STATEMENT,
    AST_IF_STATEMENT,
    AST_BREAK_STATEMENT,
    AST_CONTINUE_STATEMENT,
    AST_FUNC_DEF_STATEMENT,
    AST_RETURN_STATEMENT,
    AST_STRUCT_DEF_STATEMENT,
    AST_CONSTRUCTOR_DEF,
    AST_AS_FUNC_DEV,
    AST_NEW_STATEMENT,

    // expressions
    AST_INFIX_EXPRESSION
} CometASTNodeType;

// Main ASTNode struct, this can hold the different types of nodes.
typedef struct CometASTNode CometASTNode;
typedef CometASTNode* astNodePtr;

UseList(astNodePtr);

struct CometASTNode {
    CometASTNodeType nodeType;
    union {
        struct AST_INT { int64_t number; } AST_INT;
        struct AST_DOUBLE { double number; } AST_DOUBLE;
        struct AST_STRING { char* value; } AST_STRING;
        struct AST_IDENTIFIER { char* ident; } AST_IDENTIFIER;
        struct AST_FUNC_CALL { CometASTNode* ident; List(astNodePtr) args; } AST_FUNC_CALL;

        struct AST_ARG_DEF { CometASTNode* type; CometASTNode* ident; } AST_ARG_DEF;

        struct AST_PROGRAM { CometASTNode** statements; size_t numStatements; size_t statementsArraySize; } AST_PROGRAM;

        struct AST_INFIX_EXPRESSION { CometASTNode* left; CometASTNode* right; CometToken op; } AST_INFIX_EXPRESSION;

        struct AST_EXPRESSION_STATEMENT { CometASTNode* expression; } AST_EXPRESSION_STATEMENT;
        struct AST_ASSIGN_STATEMENT { CometASTNode* ident; CometASTNode* expression; CometASTNode* type; bool isMutable; } AST_ASSIGN_STATEMENT;
        struct AST_REASSIGN_STATEMENT { CometASTNode* ident; CometASTNode* expression; } AST_REASSIGN_STATEMENT;
        struct AST_WHILE_STATEMENT { CometASTNode* expression; CometASTNode* program; } AST_WHILE_STATEMENT;
        struct AST_FOR_STATEMENT {
            CometASTNode* type;
            CometASTNode* ident;
            CometASTNode* start;
            CometASTNode* end;
            CometASTNode* step;
            CometASTNode* program;
        } AST_FOR_STATEMENT;
        struct AST_IF_STATEMENT {
            CometASTNode* expression;
            CometASTNode* program;
            CometASTNode* elseProgram;
        } AST_IF_STATEMENT;
        struct AST_BREAK_STATEMENT { } AST_BREAK_STATEMENT;
        struct AST_CONTINUE_STATEMENT { } AST_CONTINUE_STATEMENT;
        struct AST_FUNC_DEF_STATEMENT {
            CometASTNode* ident;
            CometASTNode* program;
            List(astNodePtr) args;
            CometASTNode* returnType;
            bool isInline;
            CometASTNode* inlineExpr;
        } AST_FUNC_DEF_STATEMENT;
        struct AST_RETURN_STATEMENT { CometASTNode* expression; } AST_RETURN_STATEMENT;
        struct AST_STRUCT_DEF_STATEMENT {
            CometASTNode* ident;
            List(astNodePtr) fieldDefs;
            CometASTNode* constructor;
            CometASTNode* destructor;
        } AST_STRUCT_DEF_STATEMENT;
        struct AST_CONSTRUCTOR_DEF {
            CometASTNode* program;
            List(astNodePtr) args;
        } AST_CONSTRUCTOR_DEF;
        struct AST_AS_FUNC_DEV {
            CometASTNode* program;
            CometASTNode* castType;
        } AST_AS_FUNC_DEV;
        struct AST_NEW_STATEMENT {
            CometASTNode* structName;
            List(astNodePtr) args;
        } AST_NEW_STATEMENT;

    } data;  
};

// put a node on the heap
CometASTNode* allocateNode(CometASTNode parent);
char* ASTNodeTypeToCStr(CometASTNodeType nodeType);

#define AST_NODE(nodeType, ...) allocateNode((CometASTNode){nodeType, {.nodeType=(struct nodeType){__VA_ARGS__}}})