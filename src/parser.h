#pragma once
#include "ast.h"
#include "lexer.h"
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

    char* fileName;
    char* sourceCode;
} CometParser;

typedef CometParser* parserPtr;

typedef List(astNodePtr) argList;
typedef List(astNodePtr) nodeList;

Result(astNodePtr, ErrorMessage);
Result(nodeList, ErrorMessage);

typedef ResultType(astNodePtr, ErrorMessage) (*prefixFuncType)(CometParser*);
typedef ResultType(astNodePtr, ErrorMessage) (*infixFuncType)(CometParser*, CometASTNode* left);

typedef struct {
    nodeList chain;
    nodeList genericArgs;
} ParsedBaseType;

Result(ParsedBaseType, ErrorMessage);
Result(int, ErrorMessage);
Result(parserPtr, ErrorMessage);
Result(prefixFuncType, int);
Result(infixFuncType, int);
Result(argList, ErrorMessage);

typedef enum {
    PRECEDENCE_LOWEST = 1,
    PRECEDENCE_OR = 2,
    PRECEDENCE_AND = 3,
    PRECEDENCE_EQUALS = 4,
    PRECEDENCE_LESSGREATER = 5,
    PRECEDENCE_SUM = 6,
    PRECEDENCE_PRODUCT = 7,
    PRECEDENCE_EXPONENT = 8,
    PRECEDENCE_PREFIX = 9,
    PRECEDENCE_CALL = 10,
    PRECEDENCE_SET = 11,
    PRECEDENCE_INDEX = 12,
    PRECEDENCE_DOT = 13
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
ResultType(parserPtr, ErrorMessage) newParser(tokenList tokens, char* fileName, char* sourceCode);
// parse tokens via lexer and return the AST
ResultType(astNodePtr, ErrorMessage) buildAST(CometParser* parser);

void printNode(CometASTNode* node);