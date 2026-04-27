#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "token.h"


const CometTokenPrecedencePair PRECEDENCES[] = {
    {CT_PLUS, PRECEDENCE_SUM},
    {CT_MINUS, PRECEDENCE_SUM},
    {CT_TIMES, PRECEDENCE_PRODUCT},
    {CT_DIVIDE, PRECEDENCE_PRODUCT},
    {CT_MOD, PRECEDENCE_PRODUCT},
    {CT_POW, PRECEDENCE_EXPONENT},
};

CometASTNode* parseIntLiteral(CometParser* parser);
CometASTNode* parseFloatLiteral(CometParser* parser);
CometASTNode* parseIdentifierLiteral(CometParser* parser);
CometASTNode* parseGroupedExpression(CometParser* parser);
const CometPrefixParseFn PREFIX_PARSE_FUNCTIONS[] = {
    {CT_INT_LITERAL, parseIntLiteral},
    {CT_FLOAT_LITERAL, parseFloatLiteral},
    {CT_OPEN_PAREN, parseGroupedExpression},
    {CT_IDENT, parseIdentifierLiteral}
};

CometASTNode* parseInfixExpression(CometParser* parser, CometASTNode* left);
const CometInfixParseFn INFIX_PARSE_FUNCTIONS[] = {
    {CT_PLUS, parseInfixExpression},
    {CT_MINUS, parseInfixExpression},
    {CT_TIMES, parseInfixExpression},
    {CT_DIVIDE, parseInfixExpression},
    {CT_MOD, parseInfixExpression},
    {CT_POW, parseInfixExpression},
};

// -- HELPER METHODS -- //

// create a new CometParser instance
CometParser* newCometParser(tokenList tokens) {
    CometParser* parser = malloc(sizeof(CometParser));

    parser->tokens = tokens;
    parser->currentToken = get(tokens, 0);
    parser->peekToken = get(tokens, 1);
    parser->tokenIndex = 0;
    parser->statementIndex = 0;
    parser->statementArraySize = 8;
    parser->program = AST_NODE(
        AST_PROGRAM,
        calloc(parser->statementArraySize, sizeof(CometASTNode*)),
        0
    );

    // create an environment with no parent
    parser->environment = newEnvironment("root", NULL);
    
    return parser;
}

void parserNextToken(CometParser* parser) {
    parser->tokenIndex++;
    parser->currentToken = parser->peekToken;
    parser->peekToken = get(parser->tokens, parser->tokenIndex);
}

// add a statement to the statements list of the program, accounting for memory and whatnot
void appendStatement(CometParser* parser, CometASTNode* statement) {
    parser->program->data.AST_PROGRAM.statements[parser->statementIndex++] = statement;
    parser->program->data.AST_PROGRAM.numStatements++;

    // ensure we realloc to make space if we run out, doubling the number of max
    // statements each time
    if (parser->statementIndex >= parser->statementArraySize) {
        parser->statementArraySize *= 2;
        parser->program->data.AST_PROGRAM.statements = realloc(parser->program->data.AST_PROGRAM.statements, sizeof(CometASTNode*) * parser->statementArraySize);
    }
}

bool currentTokenIs(CometParser* parser, CometTokenType tokenType) {
    return parser->currentToken->type == tokenType;
}

bool peekTokenIs(CometParser* parser, CometTokenType tokenType) {
    return parser->peekToken->type == tokenType;
}

bool expectPeek(CometParser* parser, CometTokenType tokenType) {
    // TODO
    if (!peekTokenIs(parser, tokenType)) {
        printf("Expected next token to be %s but got %s instead.", tokenTypeToCStr(tokenType), tokenTypeToCStr(parser->peekToken->type));
        return false;
    }
    parserNextToken(parser);
    return true;
}

CometPrecedenceType getPrecedence(CometTokenType tokenType) {
    for (size_t i = 0; i < sizeof(PRECEDENCES)/sizeof(PRECEDENCES[0]); i++) {
        if (PRECEDENCES[i].tokenType == tokenType) {
            return PRECEDENCES[i].precedenceType;
        }
    }

    return PRECEDENCE_LOWEST;
}

CometPrecedenceType currentPrecedence(CometParser* parser) {
    return getPrecedence(parser->currentToken->type);
}

CometPrecedenceType peekPrecedence(CometParser* parser) {
    return getPrecedence(parser->peekToken->type);
}

prefixFuncType getPrefixFunc(CometTokenType tokenType) {
    for (size_t i = 0; i < sizeof(PREFIX_PARSE_FUNCTIONS)/sizeof(PREFIX_PARSE_FUNCTIONS[0]); i++) {
        if (PREFIX_PARSE_FUNCTIONS[i].tokenType == tokenType) {
            return PREFIX_PARSE_FUNCTIONS[i].function;
        }
    }

    return NULL;
}

infixFuncType getInfixFunc(CometTokenType tokenType) {
    for (size_t i = 0; i < sizeof(INFIX_PARSE_FUNCTIONS)/sizeof(INFIX_PARSE_FUNCTIONS[0]); i++) {
        if (INFIX_PARSE_FUNCTIONS[i].tokenType == tokenType) {
            return INFIX_PARSE_FUNCTIONS[i].function;
        }
    }

    return NULL;
}

void printNode(CometASTNode* node) {
    switch (node->nodeType) {
        case AST_PROGRAM:
            printf("Program:\n");

            CometASTNode** statements = node->data.AST_PROGRAM.statements;

            for (int i = 0; i < node->data.AST_PROGRAM.numStatements; i++) {
                printf("    %d. ", i+1);
                printNode(statements[i]);
                printf("\n");
            }
            break;

        case AST_INT: printf("%ld", node->data.AST_INT.number); break;
        case AST_DOUBLE: printf("%f", node->data.AST_DOUBLE.number); break;
        case AST_IDENTIFIER: printf("%s", node->data.AST_IDENTIFIER.ident); break;
            
        case AST_INFIX_EXPRESSION:
            printf("(");
            printNode(node->data.AST_INFIX_EXPRESSION.left);
            printf(" %s ", node->data.AST_INFIX_EXPRESSION.op);
            printNode(node->data.AST_INFIX_EXPRESSION.right);
            printf(")");
            break;


        case AST_EXPRESSION_STATEMENT:
            printNode(node->data.AST_EXPRESSION_STATEMENT.expression);
            break;
        case AST_ASSIGN_STATEMENT:
            printNode(node->data.AST_ASSIGN_STATEMENT.ident);
            printf(" = ");
            printNode(node->data.AST_ASSIGN_STATEMENT.expression);
            break;

        default:
            printf("reached unkown node type (got %d)\n", node->nodeType);
            break;
        
    }
}

// -- EXPRESSION METHODS -- //
CometASTNode* parseExpression(CometParser* parser, CometPrecedenceType precedence) {
    prefixFuncType prefixFunc = getPrefixFunc(parser->currentToken->type);

    if (prefixFunc == NULL) {
        printf("No prefix parse function for %s.\n", tokenTypeToCStr(parser->currentToken->type));
        return NULL;
    }

    CometASTNode* leftExpr = prefixFunc(parser);
    while (precedence < peekPrecedence(parser)) {
        
        infixFuncType infixFunc = getInfixFunc(parser->peekToken->type);

        if (infixFunc == NULL)
            return leftExpr;

        

        parserNextToken(parser);

        leftExpr = infixFunc(parser, leftExpr);
    }

    

    return leftExpr;
}

CometASTNode* parseInfixExpression(CometParser* parser, CometASTNode* left) {
    CometASTNode* infixExpr = AST_NODE(AST_INFIX_EXPRESSION, left, NULL, parser->currentToken->value.literal);

    CometPrecedenceType precedence = currentPrecedence(parser);
    parserNextToken(parser);

    CometASTNode* right = parseExpression(parser, precedence);

    infixExpr->data.AST_INFIX_EXPRESSION.right = right;

    return infixExpr;
}

CometASTNode* parseGroupedExpression(CometParser* parser) {
    parserNextToken(parser);

    CometASTNode* expr = parseExpression(parser, PRECEDENCE_LOWEST);

    if (!expectPeek(parser, CT_CLOSE_PAREN)) {
        return NULL;
    }

    return expr;
}

// -- PREFIX METHODS -- //
CometASTNode* parseIntLiteral(CometParser* parser) {
    return AST_NODE(AST_INT, parser->currentToken->value.intVal);
}

CometASTNode* parseFloatLiteral(CometParser* parser) {
    return AST_NODE(AST_DOUBLE, parser->currentToken->value.doubleVal);
}

CometASTNode* parseIdentifierLiteral(CometParser* parser) {
    return AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);
}

// -- STATEMENT METHODS -- //
CometASTNode* parseExpressionStatement(CometParser* parser) {
    CometASTNode* expr = parseExpression(parser, PRECEDENCE_LOWEST);
    CometASTNode* stmt = AST_NODE(AST_EXPRESSION_STATEMENT, expr);

    return stmt;
}

CometASTNode* parseAssignmentStatement(CometParser* parser) {
    // basic format:
    // small myVar = 10

    CometASTNode* ident = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

    if (!expectPeek(parser, CT_EQ)) {
        return NULL;
    }

    parserNextToken(parser);

    CometASTNode* value = parseExpression(parser, PRECEDENCE_LOWEST);

    CometASTNode* stmt = AST_NODE(AST_ASSIGN_STATEMENT, ident, value);

    
    
    
    return stmt;
}

// -- PARSER HELPERS -- //
CometASTNode* parseStatement(CometParser* parser) {
    switch (parser->currentToken->type) {
        case CT_IDENT:
            return parseAssignmentStatement(parser);

        default:
            return parseExpressionStatement(parser);
    }
}

// -- MAIN -- //
CometASTNode* buildAST(CometParser* parser) {
    
    while (parser->currentToken != NULL) {
        CometASTNode* stmt = parseStatement(parser);
        if (stmt != NULL)
            appendStatement(parser, stmt);

        parserNextToken(parser);
    }

    return parser->program;
}
