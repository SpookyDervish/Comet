#pragma once
#include "ast.h"
#include "lexer.h"
#include "environment.h"
#include "token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    tokenList tokens;
    CometToken* currentToken;
    CometToken* peekToken;
    size_t tokenIndex;

    CometASTNode* program;
    size_t statementIndex;
    size_t statementArraySize;

    CometEnvironment* environment;
} CometParser;

typedef CometParser* parserPtr;

typedef List(astNodePtr) argList;

Result(astNodePtr, charptr);

typedef ResultType(astNodePtr, charptr) (*prefixFuncType)(CometParser*);
typedef ResultType(astNodePtr, charptr) (*infixFuncType)(CometParser*, CometASTNode* left);

Result(parserPtr, charptr);
Result(int, charptr);
Result(prefixFuncType, charptr);
Result(infixFuncType, charptr);
Result(argList, charptr);

typedef enum {
    PRECEDENCE_LOWEST = 1,
    PRECEDENCE_EQUALS = 2,
    PRECEDENCE_LESSGREATER = 3,
    PRECEDENCE_SUM = 4,
    PRECEDENCE_PRODUCT = 5,
    PRECEDENCE_EXPONENT = 6,
    PRECEDENCE_PREFIX = 7,
    PRECEDENCE_CALL = 8,
    PRECEDENCE_INDEX = 9,
} CometPrecedenceType;


typedef struct {
    CometTokenType tokenType;
    CometPrecedenceType precedenceType;
} CometTokenPrecedencePair;
extern const CometTokenPrecedencePair PRECEDENCES[];

typedef struct {
    CometTokenType tokenType;
    prefixFuncType function;
} CometPrefixParseFn;
extern const CometPrefixParseFn PREFIX_PARSE_FUNCTIONS[];

typedef struct {
    CometTokenType tokenType;
    infixFuncType function;
} CometInfixParseFn;
extern const CometInfixParseFn INFIX_PARSE_FUNCTIONS[];

// allocate a new Parser
ResultType(parserPtr, charptr) newParser(tokenList tokens);
// parse tokens via lexer and return the AST
ResultType(astNodePtr, charptr) buildAST(CometParser* parser);

void printNode(CometASTNode* node);