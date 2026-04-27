#pragma once
#include "ast.h"
#include "lexer.h"
#include "environment.h"
#include "token.h"

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

typedef CometASTNode*(*prefixFuncType)(CometParser*);
typedef CometASTNode* (*infixFuncType)(CometParser*, CometASTNode* left);

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
CometParser* newParser(CometLexer* lexer);
// parse tokens via lexer and return the AST
CometASTNode* buildAST(CometParser* parser);

void printNode(CometASTNode* node);