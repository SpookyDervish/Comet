#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

ResultType(astNodePtr, charptr) parseIntLiteral(CometParser* parser);
ResultType(astNodePtr, charptr) parseFloatLiteral(CometParser* parser);
ResultType(astNodePtr, charptr) parseTypeName(CometParser* parser);
ResultType(astNodePtr, charptr) parseGroupedExpression(CometParser* parser);
const CometPrefixParseFn PREFIX_PARSE_FUNCTIONS[] = {
    {CT_INT_LITERAL, parseIntLiteral},
    {CT_FLOAT_LITERAL, parseFloatLiteral},
    {CT_OPEN_PAREN, parseGroupedExpression},
    {CT_TYPE_NAME, parseTypeName},
};

ResultType(astNodePtr, charptr) parseInfixExpression(CometParser* parser, CometASTNode* left);
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
ResultType(parserPtr, charptr) newParser(tokenList tokens) {
    CometParser* parser = malloc(sizeof(CometParser));
    if (!parser) {
        return Error(parserPtr, charptr, "newCometParser: failed to allocate memory for new parser!");
    }

    if (tokens.count == 0) {
        return Error(parserPtr, charptr, "newCometParser: tokens list is empty!");
    }

    parser->tokens = tokens;
    parser->currentToken = get(tokens, 0);

    if (tokens.count > 1) {
        parser->peekToken = get(tokens, 1);
    } else {
        parser->peekToken = NULL;
    }

    parser->tokenIndex = 0;
    parser->statementIndex = 0;
    parser->statementArraySize = 8;
    parser->program = AST_NODE(
        AST_PROGRAM,
        calloc(parser->statementArraySize, sizeof(CometASTNode*)),
        0
    );

    if (!parser->program->data.AST_PROGRAM.statements) {
        return Error(parserPtr, charptr, "newCometParser: Failed to allocate memory for program node statements!");
    }

    // create an environment with no parent
    parser->environment = newEnvironment("root", NULL);
    
    return Success(parserPtr, charptr, parser);
}

void parserNextToken(CometParser* parser) {
    parser->currentToken = parser->peekToken;

    parser->tokenIndex++;
    if (parser->tokenIndex + 1 >= parser->tokens.count) {
        parser->peekToken = malloc(sizeof(CometToken));
        *parser->peekToken = (CometToken){
            .type = CT_EOF,
            .literalType = CL_INT,
            .value.intVal = 0
        };
    } else {
        parser->peekToken = get(parser->tokens, parser->tokenIndex+1);
    }
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

ResultType(int, charptr) expectPeek(CometParser* parser, CometTokenType tokenType) {
    if (!parser->peekToken) {
        char* buffer = malloc(256);
        sprintf(buffer, "Expected next token to be %s but got <EOF> instead.", tokenTypeToCStr(tokenType));
        return Error(int, charptr, buffer);
    }
    
    if (!peekTokenIs(parser, tokenType)) {
        char* buffer = malloc(256);
        sprintf(buffer, "Expected next token to be %s but got %s instead.", tokenTypeToCStr(tokenType), tokenTypeToCStr(parser->peekToken->type));
        return Error(int, charptr, buffer);
    }
    parserNextToken(parser);
    return Success(int, charptr, 1);
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

ResultType(prefixFuncType, charptr) getPrefixFunc(CometTokenType tokenType) {
    for (size_t i = 0; i < sizeof(PREFIX_PARSE_FUNCTIONS)/sizeof(PREFIX_PARSE_FUNCTIONS[0]); i++) {
        if (PREFIX_PARSE_FUNCTIONS[i].tokenType == tokenType) {
            return Success(prefixFuncType, charptr, PREFIX_PARSE_FUNCTIONS[i].function);
        }
    }

    return Error(prefixFuncType, charptr, "getPrefixFunc: no prefix function found");
}

ResultType(infixFuncType, charptr) getInfixFunc(CometTokenType tokenType) {
    for (size_t i = 0; i < sizeof(INFIX_PARSE_FUNCTIONS)/sizeof(INFIX_PARSE_FUNCTIONS[0]); i++) {
        if (INFIX_PARSE_FUNCTIONS[i].tokenType == tokenType) {
            return Success(infixFuncType, charptr, INFIX_PARSE_FUNCTIONS[i].function);
        }
    }

    return Error(infixFuncType, charptr, "getInfixFunc: no infix function found");
}

void printNode(CometASTNode* node) {
    switch (node->nodeType) {
        case AST_PROGRAM:
            printf("Program:\n");

            CometASTNode** statements = node->data.AST_PROGRAM.statements;

            for (size_t i = 0; i < node->data.AST_PROGRAM.numStatements; i++) {
                printf("    %ld. ", i+1);
                printNode(statements[i]);
                printf("\n");
            }
            break;

        case AST_INT: printf("%ld", node->data.AST_INT.number); break;
        case AST_DOUBLE: printf("%f", node->data.AST_DOUBLE.number); break;
        case AST_IDENTIFIER: printf("%s", node->data.AST_IDENTIFIER.ident); break;
        case AST_TYPE_NAME: printf("%s", node->data.AST_TYPE_NAME.name); break;
            
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
            printNode(node->data.AST_ASSIGN_STATEMENT.type);
            printf(" ");
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
ResultType(astNodePtr, charptr) parseExpression(CometParser* parser, CometPrecedenceType precedence) {
    ResultType(prefixFuncType, charptr) prefixFunc = getPrefixFunc(parser->currentToken->type);

    if (prefixFunc.error) {
        char* buffer = malloc(256);
        sprintf(buffer, "No prefix parse function for %s.\n", tokenTypeToCStr(parser->currentToken->type));
        return Error(astNodePtr, charptr, buffer);
    }

    ResultType(astNodePtr, charptr) leftExpr = prefixFunc.as.success(parser);
    while (precedence < peekPrecedence(parser)) {
        
        ResultType(infixFuncType, charptr) infixFunc = getInfixFunc(parser->peekToken->type);

        if (infixFunc.error)
            return leftExpr;

        

        parserNextToken(parser);

        leftExpr = infixFunc.as.success(parser, leftExpr.as.success);
    }

    

    return leftExpr;
}

ResultType(astNodePtr, charptr) parseInfixExpression(CometParser* parser, CometASTNode* left) {
    CometASTNode* infixExpr = AST_NODE(AST_INFIX_EXPRESSION, left, NULL, parser->currentToken->value.literal);

    CometPrecedenceType precedence = currentPrecedence(parser);
    parserNextToken(parser);

    ResultType(astNodePtr, charptr) right = parseExpression(parser, precedence);

    infixExpr->data.AST_INFIX_EXPRESSION.right = right.as.success;

    return Success(astNodePtr, charptr, infixExpr);
}

ResultType(astNodePtr, charptr) parseGroupedExpression(CometParser* parser) {
    parserNextToken(parser);

    ResultType(astNodePtr, charptr) expr = parseExpression(parser, PRECEDENCE_LOWEST);
    ResultType(int, charptr) expect = expectPeek(parser, CT_CLOSE_PAREN);

    if (expect.error) {
        return Error(astNodePtr, charptr, expect.as.error);
    }

    return expr;
}

// -- PREFIX METHODS -- //
ResultType(astNodePtr, charptr) parseIntLiteral(CometParser* parser) {
    return Success(astNodePtr, charptr, AST_NODE(AST_INT, parser->currentToken->value.intVal));
}

ResultType(astNodePtr, charptr) parseFloatLiteral(CometParser* parser) {
    return Success(astNodePtr, charptr, AST_NODE(AST_DOUBLE, parser->currentToken->value.doubleVal));
}

ResultType(astNodePtr, charptr) parseTypeName(CometParser* parser) {
    return Success(astNodePtr, charptr, AST_NODE(AST_TYPE_NAME, parser->currentToken->value.literal));
}

// -- STATEMENT METHODS -- //
ResultType(astNodePtr, charptr) parseExpressionStatement(CometParser* parser) {
    ResultType(astNodePtr, charptr) expr = parseExpression(parser, PRECEDENCE_LOWEST);
    
    if (expr.error) {
        return expr;
    }
    
    CometASTNode* stmt = AST_NODE(AST_EXPRESSION_STATEMENT, expr.as.success);

    return Success(astNodePtr, charptr, stmt);
}

ResultType(astNodePtr, charptr) parseAssignmentStatement(CometParser* parser) {
    // basic format:
    // small myVar = 10

        
    CometASTNode* type = AST_NODE(AST_TYPE_NAME, parser->currentToken->value.literal);

    ResultType(int, charptr) expected = expectPeek(parser, CT_IDENT);
    if (expected.error) {
        return Error(astNodePtr, charptr, expected.as.error);
    }

    CometASTNode* ident = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

    expected = expectPeek(parser, CT_EQ);

    if (expected.error) {
        return Error(astNodePtr, charptr, expected.as.error);
    }

    parserNextToken(parser);

    ResultType(astNodePtr, charptr) value = parseExpression(parser, PRECEDENCE_LOWEST);
    if (value.error) {
        return value;
    }

    CometASTNode* stmt = AST_NODE(AST_ASSIGN_STATEMENT, ident, value.as.success, type);
    
    return Success(astNodePtr, charptr, stmt);
}

// -- PARSER HELPERS -- //
ResultType(astNodePtr, charptr) parseStatement(CometParser* parser) {
    switch (parser->currentToken->type) {
        case CT_TYPE_NAME:
            return parseAssignmentStatement(parser);

        default:
            return parseExpressionStatement(parser);
    }
}

// -- MAIN -- //
ResultType(astNodePtr, charptr) buildAST(CometParser* parser) {
    
    while (parser->currentToken->type != CT_EOF) {
        ResultType(astNodePtr, charptr) stmt = parseStatement(parser);
        if (stmt.error)
            return stmt;

        appendStatement(parser, stmt.as.success);
        parserNextToken(parser);
    }

    return Success(astNodePtr, charptr, parser->program);
}
