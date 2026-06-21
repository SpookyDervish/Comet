#include "parser.h"
#include "ast.h"
#include "../include/error_message.h"
#include "lexer.h"
#include "token.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Result(int, charptr);

ResultType(astNodePtr, ErrorMessage) parseStatement(CometParser* parser, bool isMutable, FieldAttribute attrib);

const CometTokenPrecedencePair PRECEDENCES[] = {
    {CT_PLUS, PRECEDENCE_SUM},
    {CT_MINUS, PRECEDENCE_SUM},
    {CT_TIMES, PRECEDENCE_PRODUCT},
    {CT_DIVIDE, PRECEDENCE_PRODUCT},
    {CT_MOD, PRECEDENCE_PRODUCT},
    {CT_POW, PRECEDENCE_EXPONENT},
    {CT_PLUS_EQ, PRECEDENCE_SET},
    {CT_MINUS_EQ, PRECEDENCE_SET},
    {CT_TIMES_EQ, PRECEDENCE_SET},
    {CT_DIVIDE_EQ, PRECEDENCE_SET},
    {CT_MOD_EQ, PRECEDENCE_SET},
    {CT_POW_EQ, PRECEDENCE_SET},
    {CT_LT, PRECEDENCE_LESSGREATER},
    {CT_GT, PRECEDENCE_LESSGREATER},
    {CT_LTE, PRECEDENCE_LESSGREATER},
    {CT_GTE, PRECEDENCE_LESSGREATER},
    {CT_EQ_EQ, PRECEDENCE_EQUALS},
    {CT_NOT_EQ, PRECEDENCE_EQUALS},
    {CT_DOT, PRECEDENCE_INDEX},
    {CT_HASH, PRECEDENCE_INDEX},
    {CT_EQ, PRECEDENCE_SET},
    {CT_OPEN_PAREN, PRECEDENCE_CALL},
    {CT_COLON, PRECEDENCE_INDEX},
};

ResultType(astNodePtr, ErrorMessage) parseIntLiteral(CometParser* parser);
ResultType(astNodePtr, ErrorMessage) parseIdentifier(CometParser* parser);
ResultType(astNodePtr, ErrorMessage) parseFloatLiteral(CometParser* parser);
ResultType(astNodePtr, ErrorMessage) parseBoolLiteral(CometParser* parser);
ResultType(astNodePtr, ErrorMessage) parseStringLiteral(CometParser* parser);
ResultType(astNodePtr, ErrorMessage) parseArrayLiteral(CometParser* parser);
ResultType(astNodePtr, ErrorMessage) parseType(CometParser* parser);
ResultType(astNodePtr, ErrorMessage) parseGroupedExpression(CometParser* parser);
ResultType(astNodePtr, ErrorMessage) parseStructCreateStatement(CometParser* parser);
ResultType(astNodePtr, ErrorMessage) parsePrefixExpression(CometParser* parser);
const CometPrefixParseFn PREFIX_PARSE_FUNCTIONS[] = {
    {CT_INT_LITERAL, parseIntLiteral},
    {CT_FLOAT_LITERAL, parseFloatLiteral},
    {CT_STRING_LITERAL, parseStringLiteral},
    {CT_BOOL_LITERAL, parseBoolLiteral},
    {CT_IDENT, parseIdentifier},
    {CT_OPEN_PAREN, parseGroupedExpression},
    {CT_OPEN_SQUARE, parseArrayLiteral},
    {CT_KEYWORD, parseStructCreateStatement},

    {CT_NOT, parsePrefixExpression},
    {CT_HASH, parsePrefixExpression},
};

ResultType(astNodePtr, ErrorMessage) parseFunctionCall(CometParser* parser, CometASTNode* left);
ResultType(astNodePtr, ErrorMessage) parseInfixExpression(CometParser* parser, CometASTNode* left);
const CometInfixParseFn INFIX_PARSE_FUNCTIONS[] = {
    {CT_EQ, parseInfixExpression},
    {CT_PLUS, parseInfixExpression},
    {CT_MINUS, parseInfixExpression},
    {CT_TIMES, parseInfixExpression},
    {CT_DIVIDE, parseInfixExpression},
    {CT_MOD, parseInfixExpression},
    {CT_POW, parseInfixExpression},
    {CT_PLUS_EQ, parseInfixExpression},
    {CT_MINUS_EQ, parseInfixExpression},
    {CT_TIMES_EQ, parseInfixExpression},
    {CT_DIVIDE_EQ, parseInfixExpression},
    {CT_MOD_EQ, parseInfixExpression},
    {CT_POW_EQ, parseInfixExpression},
    {CT_LT, parseInfixExpression},
    {CT_GT, parseInfixExpression},
    {CT_LTE, parseInfixExpression},
    {CT_GTE, parseInfixExpression},
    {CT_EQ_EQ, parseInfixExpression},
    {CT_NOT_EQ, parseInfixExpression},
    {CT_DOT, parseInfixExpression},
    {CT_EQ, parseInfixExpression},
    {CT_COLON, parseInfixExpression},
    {CT_OPEN_PAREN, parseFunctionCall}
};

// -- HELPER METHODS -- //

// create a new CometParser instance
ResultType(parserPtr, ErrorMessage) newParser(tokenList tokens, char* fileName, char* sourceCode) {
    CometParser* parser = malloc(sizeof(CometParser));
    if (!parser) {
        ErrorMessage errMsg = createError(
            fileName,
            sourceCode, 
            "MemoryAllocFail",
            "newCometParser: failed to allocate memory for new parser",
            NULL,
            1,
            1,
            1
        );

        return Error(parserPtr, ErrorMessage, errMsg);
    }

    if (tokens.count == 0) {
        ErrorMessage errMsg = createError(
            fileName,
            sourceCode, 
            "InvalidSyntax",
            "File is empty",
            NULL,
            1,
            1,
            1
        );

        return Error(parserPtr, ErrorMessage, errMsg);
    }

    parser->fileName = fileName;
    parser->sourceCode = sourceCode;

    parser->tokens = tokens;
    parser->currentToken = get(tokens, 0);

    if (tokens.count > 1) {
        parser->peekToken = get(tokens, 1);
    } else {
        CometToken* eof = malloc(sizeof(CometToken));
        *eof = (CometToken){ .type = CT_EOF };
        parser->peekToken = eof;
    }

    parser->tokenIndex = 0;
    parser->statementIndex = 0;
    parser->statementArraySize = 8;
    parser->program = AST_NODE(
        AST_PROGRAM,
        1,
        calloc(parser->statementArraySize, sizeof(CometASTNode*)),
        0,
        parser->statementArraySize
    );

    if (!parser->program->data.AST_PROGRAM.statements) {
        ErrorMessage errMsg = createError(
            fileName,
            sourceCode, 
            "MemoryAllocFail",
            "newCometParser: Failed to allocate memory for program node statements",
            NULL,
            1,
            1,
            1
        );

        return Error(parserPtr, ErrorMessage, errMsg);
    }
    
    return Success(parserPtr, ErrorMessage, parser);
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
void appendStatement(CometASTNode* program, CometASTNode* statement) {
    // ensure we realloc to make space if we run out, doubling the number of max
    // statements each time
    if (program->data.AST_PROGRAM.numStatements+1 >= program->data.AST_PROGRAM.statementsArraySize) {
        program->data.AST_PROGRAM.statementsArraySize *= 2;
        program->data.AST_PROGRAM.statements = realloc(program->data.AST_PROGRAM.statements, sizeof(CometASTNode*) * program->data.AST_PROGRAM.statementsArraySize);
    }

    program->data.AST_PROGRAM.statements[program->data.AST_PROGRAM.numStatements] = statement;
    program->data.AST_PROGRAM.numStatements++;
}

bool currentTokenIs(CometParser* parser, CometTokenType tokenType) {
    return parser->currentToken->type == tokenType;
}

bool peekTokenIs(CometParser* parser, CometTokenType tokenType) {
    return parser->peekToken->type == tokenType;
}

bool currentTokenIsAssignment(CometParser* parser) {
    CometTokenType assignmentOperators[] = {
        CT_PLUS_EQ,
        CT_MINUS_EQ,
        CT_TIMES_EQ,
        CT_DIVIDE_EQ,
        CT_MOD_EQ,
        CT_POW_EQ,
        CT_EQ
    };

    for (size_t i = 0; i < sizeof(assignmentOperators)/sizeof(assignmentOperators[0]); i++) {
        if (currentTokenIs(parser, assignmentOperators[i])) {
            return true;
        }
    }

    return false;
}

ResultType(int, ErrorMessage) expectPeek(CometParser* parser, CometTokenType tokenType) {
    if (!parser->peekToken || parser->peekToken->type == CT_EOF) {
        char* buffer = malloc(256);
        sprintf(buffer, "Expected next token to be %s but got <EOF> instead.", tokenTypeToCStr(tokenType));

        ErrorMessage errMsg = createError(
            parser->fileName,
            parser->sourceCode, 
            "InvalidSyntax",
            buffer,
            NULL,
            parser->currentToken->lineNum,
            parser->currentToken->endCol,
            parser->currentToken->endCol
        );

        return Error(int, ErrorMessage, errMsg);
    }

    if (!peekTokenIs(parser, tokenType)) {
        char* buffer = malloc(256);
        sprintf(buffer, "Expected next token to be %s but got %s instead.", tokenTypeToCStr(tokenType), tokenTypeToCStr(parser->peekToken->type));

        ErrorMessage errMsg = createError(
            parser->fileName,
            parser->sourceCode, 
            "InvalidSyntax",
            buffer,
            NULL,
            parser->peekToken->lineNum,
            parser->peekToken->startCol,
            parser->peekToken->endCol
        );

        return Error(int, ErrorMessage, errMsg);
    }
    parserNextToken(parser);
    return Success(int, ErrorMessage, 1);
}

ResultType(int, ErrorMessage) expectPeekKeyword(CometParser* parser, const char* keyword) {

    ResultType(int, ErrorMessage) next = expectPeek(parser, CT_KEYWORD);
    if (next.error) {
        return next;
    }

    if (!parser->peekToken) {
        char* buffer = malloc(128);
        snprintf(buffer, 128, "Expected next token to be %s but got <EOF> instead.", keyword);

        ErrorMessage errMsg = createError(
            parser->fileName,
            parser->sourceCode, 
            "InvalidSyntax",
            buffer,
            NULL,
            parser->currentToken->lineNum,
            parser->currentToken->endCol,
            parser->currentToken->endCol
        );

        return Error(int, ErrorMessage, errMsg);
    }

    if (strcmp(parser->currentToken->value.literal, keyword) != 0) {
        char* buffer = malloc(128);
        snprintf(buffer, 128, "Expected next token to be %s but got %s instead.", keyword, parser->peekToken->value.literal);

        ErrorMessage errMsg = createError(
            parser->fileName,
            parser->sourceCode, 
            "InvalidSyntax",
            buffer,
            NULL,
            parser->currentToken->lineNum,
            parser->currentToken->startCol,
            parser->currentToken->endCol
        );

        return Error(int, ErrorMessage, errMsg);
    }

    return Success(int, ErrorMessage, 1);
}

bool peekKeywordIs(CometParser* parser, const char* keyword) {
    bool next = peekTokenIs(parser, CT_KEYWORD);
    if (!next) {
        return false;
    }
    return strcmp(parser->peekToken->value.literal, keyword) == 0;
}

FieldAttribute fieldAttribStringToAttribEnum(char* str) {
    if (strcmp(str, "public") == 0) {
        return FIELD_PUBLIC;
    } else if (strcmp(str, "private") == 0) {
        return FIELD_PRIVATE;
    } else if (strcmp(str, "protected") == 0) {
        return FIELD_PROTECTED;
    } else if (strcmp(str, "readonly") == 0) {
        return FIELD_READ_ONLY;
    } else {
        return FIELD_PUBLIC;
    }
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

ResultType(prefixFuncType, int) getPrefixFunc(CometTokenType tokenType) {
    for (size_t i = 0; i < sizeof(PREFIX_PARSE_FUNCTIONS)/sizeof(PREFIX_PARSE_FUNCTIONS[0]); i++) {
        if (PREFIX_PARSE_FUNCTIONS[i].tokenType == tokenType) {
            return Success(prefixFuncType, int, PREFIX_PARSE_FUNCTIONS[i].function);
        }
    }

    return Error(prefixFuncType, int, 1);
}

ResultType(infixFuncType, int) getInfixFunc(CometTokenType tokenType) {
    for (size_t i = 0; i < sizeof(INFIX_PARSE_FUNCTIONS)/sizeof(INFIX_PARSE_FUNCTIONS[0]); i++) {
        if (INFIX_PARSE_FUNCTIONS[i].tokenType == tokenType) {
            return Success(infixFuncType, int, INFIX_PARSE_FUNCTIONS[i].function);
        }
    }

    return Error(infixFuncType, int, 1);
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

        case AST_INT: printf("%zu", node->data.AST_INT.number); break;
        case AST_BOOL: printf("%s", node->data.AST_BOOL.value ? "true" : "false"); break;
        case AST_DOUBLE: printf("%f", node->data.AST_DOUBLE.number); break;
        case AST_STRING: printf("\"%s\"", node->data.AST_STRING.value); break;
        case AST_IDENTIFIER: printf("%s", node->data.AST_IDENTIFIER.ident); break;
        case AST_ARRAY:
            printf("[");

            for (size_t i = 0; i < node->data.AST_ARRAY.elements.count; i++) {
                printNode(*get(node->data.AST_ARRAY.elements, i));

                if (i < node->data.AST_ARRAY.elements.count-1) {
                    printf(", ");
                }
            }

            printf("]");
            break;
            
        case AST_TYPE: {
            printNode(node->data.AST_TYPE.baseType);

            if (node->data.AST_TYPE.dimensions > 0) {
                printf("[");
                for (size_t i = 0; i < node->data.AST_TYPE.shape.count; i++) {
                    CometASTNode* size = *get(node->data.AST_TYPE.shape, i);
                    printNode(size);

                    if (i < node->data.AST_TYPE.shape.count - 1) {
                        printf(", ");
                    } else {
                        printf("]");
                    }
                }
            }

            break;
        }

        case AST_INFIX_EXPRESSION:
            printf("(");
            printNode(node->data.AST_INFIX_EXPRESSION.left);
            printf(" %s ", node->data.AST_INFIX_EXPRESSION.op.value.literal);
            printNode(node->data.AST_INFIX_EXPRESSION.right);
            printf(")");
            break;

        case AST_PREFIX_EXPRESSION:
            printf("%s", node->data.AST_PREFIX_EXPRESSION.op.value.literal);
            printNode(node->data.AST_PREFIX_EXPRESSION.right);
            break;

        case AST_FUNC_CALL:
            printNode(node->data.AST_FUNC_CALL.ident);
            printf("(");
            for (size_t i = 0; i < node->data.AST_FUNC_CALL.args.count; i++) {
                CometASTNode* arg = *get(node->data.AST_FUNC_CALL.args, i);
                printNode(arg);

                if (i < node->data.AST_FUNC_CALL.args.count-1)
                    printf(", ");
            }
            printf(")");
            break;

        case AST_EXPRESSION_STATEMENT:
            printNode(node->data.AST_EXPRESSION_STATEMENT.expression);
            break;
        case AST_ASSIGN_STATEMENT: {
            FieldAttribute attrib = node->data.AST_ASSIGN_STATEMENT.attrib;
            if (attrib != FIELD_PUBLIC) {
                switch (attrib) {
                    case FIELD_PRIVATE: printf("private "); break;
                    case FIELD_PROTECTED: printf("protected "); break;
                    case FIELD_READ_ONLY: printf("readonly "); break;
                    default: break;
                }
            }

            printNode(node->data.AST_ASSIGN_STATEMENT.type);
            printf(" ");
            printNode(node->data.AST_ASSIGN_STATEMENT.ident);

            CometASTNode* value = node->data.AST_ASSIGN_STATEMENT.expression;

            if (value) {
                printf(" = ");
                printNode(value);
            }
            break;
        }
        case AST_REASSIGN_STATEMENT:
            printNode(node->data.AST_REASSIGN_STATEMENT.ident);
            printf(" %s ", node->data.AST_REASSIGN_STATEMENT.op.value.literal);
            printNode(node->data.AST_REASSIGN_STATEMENT.expression);
            break;
        case AST_WHILE_STATEMENT:
            printf("while ");
            printNode(node->data.AST_WHILE_STATEMENT.expression);
            printf(" {\n");
            printNode(node->data.AST_WHILE_STATEMENT.program);
            printf("       }");
            break;
        case AST_BREAK_STATEMENT:
            printf("break");
            break;
        case AST_CONTINUE_STATEMENT:
            printf("continue");
            break;
        case AST_ARG_DEF:
            printNode(node->data.AST_ARG_DEF.type);
            printf(" ");
            printNode(node->data.AST_ARG_DEF.ident);
            break;
        case AST_FUNC_DEF_STATEMENT:
            printf("func ");
            printNode(node->data.AST_FUNC_DEF_STATEMENT.ident);
            printf("(");

            for (size_t i = 0; i < node->data.AST_FUNC_DEF_STATEMENT.args.count; i++) {
                CometASTNode* arg = *get(node->data.AST_FUNC_DEF_STATEMENT.args, i);
                printNode(arg);

                if (i < node->data.AST_FUNC_DEF_STATEMENT.args.count-1)
                    printf(", ");
            }
            
            printf(") -> ");
            printNode(node->data.AST_FUNC_DEF_STATEMENT.returnType);
            printf(" ");

            if (node->data.AST_FUNC_DEF_STATEMENT.isInline) {
                printf("=> ");
                printNode(node->data.AST_FUNC_DEF_STATEMENT.inlineExpr);
            } else {
                printf("{\n");
                printNode(node->data.AST_FUNC_DEF_STATEMENT.program);
                printf("       }");
            }

            break;
        case AST_RETURN_STATEMENT:
            printf("return ");
            printNode(node->data.AST_RETURN_STATEMENT.expression);
            break;
        case AST_IF_STATEMENT:
            printf("if ");
            printNode(node->data.AST_IF_STATEMENT.expression);
            printf(" {\n");
            printNode(node->data.AST_IF_STATEMENT.program);
            printf("       } ");

            if (node->data.AST_IF_STATEMENT.elseProgram) {
                printf("else {\n");
                printNode(node->data.AST_IF_STATEMENT.elseProgram);
                printf("       } ");
            }
            break;
        case AST_FOR_STATEMENT:
            printf("for ");
            printNode(node->data.AST_FOR_STATEMENT.type);
            printf(" ");
            printNode(node->data.AST_FOR_STATEMENT.ident);
            printf(" in ");
            printNode(node->data.AST_FOR_STATEMENT.start);
            printf("..");
            printNode(node->data.AST_FOR_STATEMENT.end);
            printf(" step ");
            printNode(node->data.AST_FOR_STATEMENT.step);
            printf(" {\n");
            printNode(node->data.AST_FOR_STATEMENT.program);
            printf("       }");
            break;

        case AST_STRUCT_DEF_STATEMENT:
            printf("struct ");
            printNode(node->data.AST_STRUCT_DEF_STATEMENT.ident);
            printf(" {\n");

            List(astNodePtr) structDefs = node->data.AST_STRUCT_DEF_STATEMENT.fieldDefs;

            for (size_t i = 0; i < structDefs.count; i++) {
                printf("           ");
                printNode(*get(structDefs, i));
                printf("\n");
            }

            
            if (node->data.AST_STRUCT_DEF_STATEMENT.constructor) {
                printf("\n");
                printNode(node->data.AST_STRUCT_DEF_STATEMENT.constructor);
                printf("\n");
            }

            printf("       }");
            break;
        case AST_CONSTRUCTOR_DEF: 
            printf("           init(");
            for (size_t i = 0; i < node->data.AST_CONSTRUCTOR_DEF.args.count; i++) {
                CometASTNode* arg = *get(node->data.AST_CONSTRUCTOR_DEF.args, i);
                printNode(arg);

                if (i < node->data.AST_CONSTRUCTOR_DEF.args.count-1)
                    printf(", ");
            }
            printf(") {\n");
            printNode(node->data.AST_CONSTRUCTOR_DEF.program);
            printf("           }");

            break;
        case AST_NEW_STATEMENT:
            printf("new ");
            printNode(node->data.AST_NEW_STATEMENT.structName);
            printf("(");
            for (size_t i = 0; i < node->data.AST_CONSTRUCTOR_DEF.args.count; i++) {
                CometASTNode* arg = *get(node->data.AST_CONSTRUCTOR_DEF.args, i);
                printNode(arg);

                if (i < node->data.AST_CONSTRUCTOR_DEF.args.count-1)
                    printf(", ");
            }
            printf(")");

            break;
        case AST_IMPORT_STATEMENT:
            printf("import ");

            List(astNodePtr) importChain = node->data.AST_IMPORT_STATEMENT.importChain;

            for (size_t i = 0; i < importChain.count; i++) {
                CometASTNode* ident = *get(importChain, i);
                printf("%s", ident->data.AST_IDENTIFIER.ident);

                if (i < importChain.count-1) {
                    printf(".");
                }
            }
            printf("\n");
            break;
        case AST_BREAKPOINT_STATEMENT:
            printf("breakpoint\n");
            break;

        default:
            printf("reached unkown node type (got %d)\n", node->nodeType);
            break;
        
    }
}

// -- EXPRESSION METHODS -- //
ResultType(astNodePtr, ErrorMessage) parseExpression(CometParser* parser, CometPrecedenceType precedence) {
    ResultType(prefixFuncType, int) prefixFunc = getPrefixFunc(parser->currentToken->type);

    if (prefixFunc.error) {
        char* buffer = malloc(128);
        sprintf(buffer, "No prefix parse function for %s.", tokenTypeToCStr(parser->currentToken->type));

        ErrorMessage errMsg = createError(
            parser->fileName,
            parser->sourceCode, 
            "InvalidSyntax",
            buffer,
            NULL,
            parser->currentToken->lineNum,
            parser->currentToken->startCol,
            parser->currentToken->endCol
        );

        return Error(astNodePtr, ErrorMessage, errMsg);
    }

   


    ResultType(astNodePtr, ErrorMessage) leftExpr = prefixFunc.as.success(parser);

    while (precedence < peekPrecedence(parser)) {
        ResultType(infixFuncType, int) infixFunc = getInfixFunc(parser->peekToken->type);

        if (infixFunc.error)
            return leftExpr;

        

        parserNextToken(parser);

        leftExpr = infixFunc.as.success(parser, leftExpr.as.success);
    }

    return leftExpr;
}

ResultType(astNodePtr, ErrorMessage) parseInfixExpression(CometParser* parser, CometASTNode* left) {
    if (currentTokenIsAssignment(parser)) {
        CometToken op = *parser->currentToken;

        parserNextToken(parser);

        ResultType(astNodePtr, ErrorMessage) right = parseExpression(parser, PRECEDENCE_LOWEST);

        CometASTNode* assign = AST_NODE(AST_REASSIGN_STATEMENT, left->lineNum, left, right.as.success, op);
        assign->startCol = left->startCol;
        assign->endCol = right.as.success->endCol;

        return Success(astNodePtr, ErrorMessage, assign);
    }

    CometASTNode* infixExpr = AST_NODE(AST_INFIX_EXPRESSION, left->lineNum, left, NULL, *parser->currentToken);
    infixExpr->startCol = left->startCol;

    CometPrecedenceType precedence = currentPrecedence(parser);
    parserNextToken(parser);

    ResultType(astNodePtr, ErrorMessage) right = parseExpression(parser, precedence);
    if (right.error)
        return right;

    infixExpr->data.AST_INFIX_EXPRESSION.right = right.as.success;
    infixExpr->endCol = right.as.success->endCol;

    return Success(astNodePtr, ErrorMessage, infixExpr);
}

ResultType(astNodePtr, ErrorMessage) parseGroupedExpression(CometParser* parser) {
    parserNextToken(parser);

    ResultType(astNodePtr, ErrorMessage) expr = parseExpression(parser, PRECEDENCE_LOWEST);
    ResultType(int, ErrorMessage) expect = expectPeek(parser, CT_CLOSE_PAREN);

    if (expect.error) {
        return Error(astNodePtr, ErrorMessage, expect.as.error);
    }

    return expr;
}

ResultType(argList, ErrorMessage) parseFunctionDefArgs(CometParser* parser) {
    List(astNodePtr) args = newList(astNodePtr);

    while (parser->currentToken->type != CT_CLOSE_PAREN) {
        if (parser->peekToken->type == CT_EOF) {
            ErrorMessage errMsg = createError(
                parser->fileName,
                parser->sourceCode, 
                "InvalidSyntax",
                "Function args were not closed!",
                NULL,
                parser->currentToken->lineNum,
                parser->currentToken->startCol,
                parser->currentToken->startCol
            );
            
            return Error(argList, ErrorMessage, errMsg);
        } else if (parser->peekToken->type == CT_CLOSE_PAREN) {
            parserNextToken(parser);
            break;
        }

        

        ResultType(int, ErrorMessage) expectType = expectPeek(parser, CT_IDENT);
        if (expectType.error) {
            return Error(argList, ErrorMessage, expectType.as.error);
        }

        ResultType(astNodePtr, ErrorMessage) type = parseType(parser);
        if (type.error)
            return Error(argList, ErrorMessage, type.as.error);

        ResultType(int, ErrorMessage) expectArgName = expectPeek(parser, CT_IDENT);
        if (expectArgName.error) {
            return Error(argList, ErrorMessage, expectArgName.as.error);
        }
        CometASTNode* argName = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, parser->currentToken->value.literal);
        argName->startCol = parser->currentToken->startCol;
        argName->endCol = parser->currentToken->endCol;

        CometASTNode* argDef = AST_NODE(AST_ARG_DEF, type.as.success->lineNum, type.as.success, argName);
        argDef->startCol = argName->startCol;
        argDef->endCol = type.as.success->endCol;
        append(args, argDef);


        ResultType(int, ErrorMessage) expectComma = expectPeek(parser, CT_COMMA);
        if (expectComma.error && !peekTokenIs(parser, CT_CLOSE_PAREN)) {
            return Error(argList, ErrorMessage, expectComma.as.error);
        }
    }

    return Success(argList, ErrorMessage, args);
}

ResultType(argList, ErrorMessage) parseFunctionCallArgs(CometParser* parser) {
    List(astNodePtr) args = newList(astNodePtr);
    parserNextToken(parser); // skip open paren '('

    while (parser->currentToken->type != CT_CLOSE_PAREN) {
        if (parser->peekToken->type == CT_EOF) {
            ErrorMessage errMsg = createError(
                parser->fileName,
                parser->sourceCode, 
                "InvalidSyntax",
                "Function args were not closed!",
                NULL,
                parser->currentToken->lineNum,
                parser->currentToken->startCol,
                parser->currentToken->startCol
            );

            return Error(argList, ErrorMessage, errMsg);
        }

        ResultType(astNodePtr, ErrorMessage) expr = parseExpression(parser, PRECEDENCE_LOWEST);
        if (expr.error) {
            return Error(argList, ErrorMessage, expr.as.error);
        }
        append(args, expr.as.success);

        parserNextToken(parser);

        // Only consume ',' if it's actually there
        if (currentTokenIs(parser, CT_COMMA)) {
            parserNextToken(parser);
        } else if (!currentTokenIs(parser, CT_CLOSE_PAREN)) {
            ErrorMessage errMsg = createError(
                parser->fileName,
                parser->sourceCode, 
                "InvalidSyntax",
                "Expected ',' or ')' after function argument",
                NULL,
                parser->currentToken->lineNum,
                parser->currentToken->startCol,
                parser->currentToken->startCol
            );

            return Error(argList, ErrorMessage, errMsg);
        }
    }

    return Success(argList, ErrorMessage, args);
}

// -- PREFIX METHODS -- //
ResultType(astNodePtr, ErrorMessage) parseIntLiteral(CometParser* parser) {
    CometASTNode* node = AST_NODE(AST_INT, parser->currentToken->lineNum, parser->currentToken->value.intVal);
    node->startCol = parser->currentToken->startCol;
    node->endCol = parser->currentToken->endCol;
    return Success(astNodePtr, ErrorMessage, node);
}

ResultType(astNodePtr, ErrorMessage) parseFloatLiteral(CometParser* parser) {
    CometASTNode* node = AST_NODE(AST_DOUBLE, parser->currentToken->lineNum, parser->currentToken->value.doubleVal);
    node->startCol = parser->currentToken->startCol;
    node->endCol = parser->currentToken->endCol;
    return Success(astNodePtr, ErrorMessage, node);
}

ResultType(astNodePtr, ErrorMessage) parseBoolLiteral(CometParser* parser) {
    CometASTNode* node = AST_NODE(AST_BOOL, parser->currentToken->lineNum, parser->currentToken->value.boolVal);
    node->startCol = parser->currentToken->startCol;
    node->endCol = parser->currentToken->endCol;
    return Success(astNodePtr, ErrorMessage, node);
}

ResultType(astNodePtr, ErrorMessage) parseStringLiteral(CometParser* parser) {
    CometASTNode* node = AST_NODE(AST_STRING, parser->currentToken->lineNum, parser->currentToken->value.literal);
    node->startCol = parser->currentToken->startCol;
    node->endCol = parser->currentToken->endCol;
    return Success(astNodePtr, ErrorMessage, node);
}

ResultType(astNodePtr, ErrorMessage) parseArrayLiteral(CometParser* parser) {
    CometASTNode* literal = AST_NODE(AST_ARRAY, parser->currentToken->lineNum, newList(astNodePtr));
    literal->startCol = parser->currentToken->startCol;
    

    while (!peekTokenIs(parser, CT_CLOSE_SQUARE)) {
        parserNextToken(parser);
        ResultType(astNodePtr, ErrorMessage) value = parseExpression(parser, PRECEDENCE_LOWEST);
        if (value.error)
            return value;

        append(literal->data.AST_ARRAY.elements, value.as.success);

        if (peekTokenIs(parser, CT_CLOSE_SQUARE)) {
            break;
        } else if (peekTokenIs(parser, CT_COMMA)) {
            parserNextToken(parser);
            continue;
        } else {
            Estr buffer = CREATE_ESTR("Expected ',' or ']', got \"");
            APPEND_ESTR(buffer, tokenTypeToCStr(parser->peekToken->type));
            APPEND_ESTR(buffer, "\" instead.");

            ErrorMessage errMsg = createError(
                parser->fileName,
                parser->sourceCode, 
                "InvalidSyntax",
                buffer.str,
                NULL,
                parser->peekToken->lineNum,
                parser->peekToken->startCol,
                parser->peekToken->endCol
            );

            return Error(astNodePtr, ErrorMessage, errMsg);
        }
    }

    literal->endCol = parser->currentToken->endCol;

    parserNextToken(parser); // skip ']'

    return Success(astNodePtr, ErrorMessage, literal);
}

ResultType(astNodePtr, ErrorMessage) parseIdentifier(CometParser* parser) {
    CometASTNode* ident = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, parser->currentToken->value.literal);
    ident->startCol = parser->currentToken->startCol;
    ident->endCol = parser->currentToken->endCol;
    return Success(astNodePtr, ErrorMessage, ident);
}

ResultType(astNodePtr, ErrorMessage) parsePrefixExpression(CometParser* parser) {
    CometASTNode* expr = AST_NODE(AST_PREFIX_EXPRESSION, parser->currentToken->lineNum, *parser->currentToken, NULL);
    expr->startCol = parser->currentToken->startCol;

    parserNextToken(parser);

    ResultType(astNodePtr, ErrorMessage) rightSide = parseExpression(parser, PRECEDENCE_INDEX);
    if (rightSide.error)
        return rightSide;

    expr->data.AST_PREFIX_EXPRESSION.right = rightSide.as.success;

    expr->endCol = rightSide.as.success->endCol;

    return Success(astNodePtr, ErrorMessage, expr);
}

// -- TYPE PARSING -- //
ResultType(astNodePtr, ErrorMessage) parseArrayType(CometParser* parser) {
    
    CometASTNode* baseType = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, parser->currentToken->value.literal);
    baseType->startCol = parser->currentToken->startCol;
    baseType->endCol = parser->currentToken->endCol;
    parserNextToken(parser); // consume base type

    CometASTNode* typeNode = AST_NODE(AST_TYPE, baseType->lineNum, baseType, newList(astNodePtr), 0);
    typeNode->startCol = baseType->startCol;

    parserNextToken(parser); // consume '['

    while (true) {

        if (parser->currentToken->type == CT_TIMES) {
            char* buffer = malloc(2);
            buffer[0] = '*';
            buffer[1] = 0;

            CometASTNode* node = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, buffer);
            node->startCol = parser->currentToken->startCol;
            node->endCol = parser->currentToken->endCol;

            append(
                typeNode->data.AST_TYPE.shape,
                node
            );

        } else {
            ResultType(astNodePtr, ErrorMessage) expr = parseExpression(parser, PRECEDENCE_LOWEST);
            if (expr.error)
                return expr;

            append(typeNode->data.AST_TYPE.shape, expr.as.success);

            
        }

        
        parserNextToken(parser);

        typeNode->data.AST_TYPE.dimensions++;

        if (currentTokenIs(parser, CT_COMMA)) {
            parserNextToken(parser);
            continue;
        } else if (currentTokenIs(parser, CT_CLOSE_SQUARE)) {
            break;
        } else {
            Estr buffer = CREATE_ESTR("Expected ',' or ']' when continuing array type, got \"");
            APPEND_ESTR(buffer, tokenTypeToCStr(parser->currentToken->type));
            APPEND_ESTR(buffer, "\" instead.");

            ErrorMessage errMsg = createError(
                parser->fileName,
                parser->sourceCode, 
                "InvalidSyntax",
                buffer.str,
                NULL,
                parser->currentToken->lineNum,
                parser->currentToken->startCol,
                parser->currentToken->endCol
            );

            return Error(astNodePtr, ErrorMessage, errMsg);
        }
    }

    typeNode->endCol = parser->currentToken->endCol;

    return Success(astNodePtr, ErrorMessage, typeNode);
}

ResultType(astNodePtr, ErrorMessage) parseScalarType(CometParser* parser) {
    if (!currentTokenIs(parser, CT_IDENT)) {
        Estr buffer = CREATE_ESTR("Expected type name, got ");
        APPEND_ESTR(buffer, tokenTypeToCStr(parser->currentToken->type));
        APPEND_ESTR(buffer, " instead");

        ErrorMessage errMsg = createError(
            parser->fileName,
            parser->sourceCode,
            "InvalidSyntax",
            buffer.str,
            NULL,
            parser->currentToken->lineNum,
            parser->currentToken->startCol,
            parser->currentToken->endCol
        );

        return Error(astNodePtr, ErrorMessage, errMsg);
    }

    CometASTNode* baseType = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, parser->currentToken->value.literal);
    baseType->startCol = parser->currentToken->startCol;
    baseType->endCol = parser->currentToken->endCol;

    CometASTNode* typeNode = AST_NODE(AST_TYPE, baseType->lineNum, baseType, NULL, 0);
    typeNode->startCol = baseType->startCol;
    typeNode->endCol = baseType->endCol;

    return Success(astNodePtr, ErrorMessage, typeNode);
}

ResultType(astNodePtr, ErrorMessage) parseType(CometParser* parser) {
    if (peekTokenIs(parser, CT_OPEN_SQUARE)) {
        return parseArrayType(parser);
    } else {
        return parseScalarType(parser);
    }
}

// -- STATEMENT METHODS -- //
ResultType(astNodePtr, ErrorMessage) parseExpressionStatement(CometParser* parser) {
    ResultType(astNodePtr, ErrorMessage) expr = parseExpression(parser, PRECEDENCE_LOWEST);
    
    if (expr.error) {
        return expr;
    }
    
    CometASTNode* stmt = AST_NODE(AST_EXPRESSION_STATEMENT, expr.as.success->lineNum, expr.as.success);
    stmt->startCol = expr.as.success->startCol;
    stmt->endCol = expr.as.success->endCol;

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseAssignmentStatement(CometParser* parser, bool isMutable, FieldAttribute fieldAttrib) {
    // basic format:
    // small myVar = 10

    if (currentTokenIs(parser, CT_KEYWORD)) {
        char* keyword = parser->currentToken->value.literal;

        if (strcmp(keyword, "mut") == 0) {
            if (isMutable) {
                ErrorMessage errMsg = createError(
                    parser->fileName,
                    parser->sourceCode, 
                    "InvalidSyntax",
                    "\"mut\" keyword appears twice in variable declaration.",
                    "Remove one \"mut\" to silence this error.",
                    parser->currentToken->lineNum,
                    parser->currentToken->startCol,
                    parser->currentToken->endCol
                );

                return Error(astNodePtr, ErrorMessage, errMsg);
            }
            
            parserNextToken(parser);
            return parseAssignmentStatement(parser, true, fieldAttrib);
        } else if (
            strcmp(keyword, "private") == 0 ||
            strcmp(keyword, "protected") == 0 ||
            strcmp(keyword, "readonly") == 0) {
            if (fieldAttrib != FIELD_PUBLIC) {
                ErrorMessage errMsg = createError(
                    parser->fileName,
                    parser->sourceCode, 
                    "InvalidSyntax",
                    "Cannot set multiple attributes to struct field.",
                    NULL,
                    parser->currentToken->lineNum,
                    parser->currentToken->startCol,
                    parser->currentToken->endCol
                );

                return Error(astNodePtr, ErrorMessage, errMsg);
            }

            parserNextToken(parser);
            return parseAssignmentStatement(parser, isMutable, attribStringToFieldAttrib(keyword));
        }
    }

    ResultType(astNodePtr, ErrorMessage) type = parseType(parser);//AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);
    if (type.error)
        return type;

    bool expectName = peekTokenIs(parser, CT_IDENT);
    if (!expectName)
        return parseExpression(parser, PRECEDENCE_LOWEST);
    parserNextToken(parser);

    CometASTNode* ident = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, parser->currentToken->value.literal);
    ident->startCol = parser->currentToken->startCol;
    ident->endCol = parser->currentToken->endCol;

    CometASTNode* stmt = AST_NODE(AST_ASSIGN_STATEMENT, type.as.success->lineNum, ident, NULL, type.as.success, isMutable, fieldAttrib);
    stmt->startCol = type.as.success->startCol;
    

    bool hasValue = peekTokenIs(parser, CT_EQ);
    

    if (!hasValue) {
        stmt->endCol = ident->endCol;
        return Success(astNodePtr, ErrorMessage, stmt);
    }

    parserNextToken(parser);
    parserNextToken(parser);

    
    

    ResultType(astNodePtr, ErrorMessage) value = parseExpression(parser, PRECEDENCE_LOWEST);
    if (value.error) {
        return value;
    }

    stmt->data.AST_ASSIGN_STATEMENT.expression = value.as.success;
    stmt->endCol = value.as.success->endCol;
    
    
    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseBlockStatement(CometParser* parser) {
    CometASTNode** statements = calloc(1, sizeof(CometASTNode*));
    if (!statements) {
        ErrorMessage errMsg = createError(
            parser->fileName,
            parser->sourceCode, 
            "MemoryAllocFail",
            "parseBlockStatement: failed to allocate memory for block statement",
            NULL,
            parser->currentToken->lineNum,
            parser->currentToken->startCol,
            parser->currentToken->endCol
        );

        return Error(astNodePtr, ErrorMessage, errMsg);
    }

    ResultType(int, ErrorMessage) openCurly = expectPeek(parser, CT_OPEN_CURLY);
    if (openCurly.error) {
        return Error(astNodePtr, ErrorMessage, openCurly.as.error);
    }

    uint32_t startCol = parser->currentToken->startCol;
    uint32_t lineNum = parser->currentToken->lineNum;

    CometASTNode* program = AST_NODE(AST_PROGRAM, parser->currentToken->lineNum, statements, 0, 1);
    program->startCol = parser->currentToken->startCol;

    parserNextToken(parser);

    while (parser->currentToken->type != CT_CLOSE_CURLY) {
        if (parser->currentToken->type == CT_EOF) {
            ErrorMessage errMsg = createError(
                parser->fileName,
                parser->sourceCode, 
                "InvalidSyntax",
                "Block statement was not closed",
                NULL,
                lineNum,
                startCol,
                startCol
            );

            return Error(astNodePtr, ErrorMessage, errMsg);
        }

        ResultType(astNodePtr, ErrorMessage) stmt = parseStatement(parser, false, FIELD_PUBLIC);
        if (stmt.error)
            return stmt;

        appendStatement(program, stmt.as.success);

        parserNextToken(parser);
        
    }

    program->endCol = parser->currentToken->endCol;
    

    return Success(astNodePtr, ErrorMessage, program);
}

ResultType(astNodePtr, ErrorMessage) parseOptionalBlockStatement(CometParser* parser) {
    // used for stuff like
    // if x + 1 == 2
    //     break

    CometASTNode* stmt;
    if (!peekTokenIs(parser, CT_OPEN_CURLY)) { // if 1+1 == 2 return

        parserNextToken(parser);
        ResultType(astNodePtr, ErrorMessage) innerStmt = parseStatement(parser, false, FIELD_PUBLIC);

        if (innerStmt.error)
            return innerStmt;

        CometASTNode** statements = calloc(1, sizeof(CometASTNode*));
        if (!statements) {
            ErrorMessage errMsg = createError(
                parser->fileName,
                parser->sourceCode, 
                "MemoryAllocFail",
                "parseOptionalBlockStatement: failed to allocate memory for block statement",
                NULL,
                parser->currentToken->lineNum,
                parser->currentToken->startCol,
                parser->currentToken->endCol
            );

            return Error(astNodePtr, ErrorMessage, errMsg);
        }

        CometASTNode* program = AST_NODE(AST_PROGRAM, innerStmt.as.success->lineNum, statements, 0, 1);
        program->startCol = innerStmt.as.success->startCol;
        program->endCol = innerStmt.as.success->endCol;
        appendStatement(program, innerStmt.as.success);

        stmt = program;
    } else {                                             // if 1+1 == 2 { return }
        
        ResultType(astNodePtr, ErrorMessage) block = parseBlockStatement(parser);
        if (block.error) {
            return block;
        }

        stmt = block.as.success;
        
    }

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseWhileStatement(CometParser* parser) {
    // basic format:
    // while true {}

    CometASTNode* stmt = AST_NODE(AST_WHILE_STATEMENT, parser->currentToken->startCol, NULL, NULL);

    parserNextToken(parser);

    ResultType(astNodePtr, ErrorMessage) expression = parseExpression(parser, PRECEDENCE_LOWEST);
    if (expression.error) {
        return expression;
    }

    stmt->data.AST_WHILE_STATEMENT.expression = expression.as.success;

    ResultType(astNodePtr, ErrorMessage) program = parseOptionalBlockStatement(parser);
    if (program.error) {
        return program;
    }

    stmt->data.AST_WHILE_STATEMENT.program = program.as.success;
    stmt->endCol = program.as.success->endCol;

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseForStatement(CometParser* parser) {
    // basic format
    // for int i in 0 .. 10 {}

    uint32_t lineNum = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    ResultType(int, ErrorMessage) expectType = expectPeek(parser, CT_IDENT);
    if (expectType.error) {
        return Error(astNodePtr, ErrorMessage, expectType.as.error);
    }

    ResultType(astNodePtr, ErrorMessage) type = parseType(parser);//AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);
    if (type.error)
        return type;

    ResultType(int, ErrorMessage) expectIdent = expectPeek(parser, CT_IDENT);
    if (expectIdent.error) {
        return Error(astNodePtr, ErrorMessage, expectIdent.as.error);
    }

    CometASTNode* ident = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, parser->currentToken->value.literal);
    ident->startCol = parser->currentToken->startCol;
    ident->endCol = parser->currentToken->endCol;
    
    

    ResultType(int, ErrorMessage) expectIn = expectPeekKeyword(parser, "in");
    if (expectIn.error) {
        return Error(astNodePtr, ErrorMessage, expectIn.as.error);
    }

    parserNextToken(parser);

    ResultType(astNodePtr, ErrorMessage) start = parseExpression(parser, PRECEDENCE_LOWEST);
    if (start.error) {
        return start;
    }

    ResultType(int, ErrorMessage) dotDot = expectPeek(parser, CT_DOT_DOT);
    if (dotDot.error) {
        return Error(astNodePtr, ErrorMessage, dotDot.as.error);
    }

    parserNextToken(parser);

    ResultType(astNodePtr, ErrorMessage) end = parseExpression(parser, PRECEDENCE_LOWEST);
    if (end.error) {
        return end;
    }

    ResultType(astNodePtr, ErrorMessage) stepNode;
    ResultType(int, ErrorMessage) step = expectPeekKeyword(parser, "step");
   
    if (!step.error) {
        parserNextToken(parser);
        stepNode = parseExpression(parser, PRECEDENCE_LOWEST);
        if (stepNode.error) {
            return stepNode;
        }
    } else {
        stepNode = Success(astNodePtr, ErrorMessage, AST_NODE(AST_INT, parser->currentToken->lineNum, 1));
    }

    ResultType(astNodePtr, ErrorMessage) block = parseOptionalBlockStatement(parser);
    if (block.error) {
        return block;
    }

    CometASTNode* stmt = AST_NODE(
        AST_FOR_STATEMENT,
        lineNum,
        type.as.success,
        ident,
        start.as.success,
        end.as.success,
        stepNode.as.success,
        block.as.success
    );
    stmt->startCol = startCol;
    stmt->endCol = block.as.success->endCol;

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseBreakStatement(CometParser* parser) {
    CometASTNode* n = AST_NODE(AST_BREAK_STATEMENT, parser->currentToken->lineNum);
    n->startCol = parser->currentToken->startCol;
    n->endCol = parser->currentToken->endCol;
    return Success(astNodePtr, ErrorMessage, n);
}

ResultType(astNodePtr, ErrorMessage) parseContinueStatement(CometParser* parser) {
    CometASTNode* n = AST_NODE(AST_CONTINUE_STATEMENT, parser->currentToken->lineNum);
    n->startCol = parser->currentToken->startCol;
    n->endCol = parser->currentToken->endCol;
    return Success(astNodePtr, ErrorMessage, n);
}

ResultType(astNodePtr, ErrorMessage) parseIfStatement(CometParser* parser) {
    // basic format
    // if 1+1 == 2 {}

    uint32_t lineNum = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    parserNextToken(parser);

    ResultType(astNodePtr, ErrorMessage) expr = parseExpression(parser, PRECEDENCE_LOWEST);
    if (expr.error) {
        return expr;
    }
    
    ResultType(astNodePtr, ErrorMessage) body = parseOptionalBlockStatement(parser);
    if (body.error)
        return body;

    CometASTNode* elseBody = NULL;

    bool elseKeyword = peekKeywordIs(parser, "else");

    if (elseKeyword) {
        parserNextToken(parser);
        ResultType(astNodePtr, ErrorMessage) elseResult = parseOptionalBlockStatement(parser);
        if (elseResult.error)
            return elseResult;

        elseBody = elseResult.as.success;
    }

    CometASTNode* stmt = AST_NODE(
        AST_IF_STATEMENT,
        lineNum,
        expr.as.success,
        body.as.success,
        elseBody
    );
    stmt->startCol = startCol;
    stmt->endCol = parser->currentToken->endCol;
     
    


    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseFunctionDefStatement(CometParser* parser, FieldAttribute fieldAttrib) {
    // basic format
    // func func_name(type arg_name, type2 arg_name2) -> return_type {
    //     return arg_name + arg_name2
    // }
    //
    // inline body func
    // func func_name(type arg_name, type2 arg_name2) => arg_name + arg_name2

    uint32_t lineNum = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    ResultType(int, ErrorMessage) expectName = expectPeek(parser, CT_IDENT);
    if (expectName.error) {
        return Error(astNodePtr, ErrorMessage, expectName.as.error);
    }

    CometASTNode* ident = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, parser->currentToken->value.literal);
    ident->startCol = parser->currentToken->startCol;
    ident->endCol = parser->currentToken->endCol;

    bool isInline = false;

    ResultType(int, ErrorMessage) expectOpenParen = expectPeek(parser, CT_OPEN_PAREN);
    if (expectOpenParen.error) {
        return Error(astNodePtr, ErrorMessage, expectOpenParen.as.error);
    }

    // parse args
    ResultType(argList, ErrorMessage) args = parseFunctionDefArgs(parser);
    if (args.error) {
        return Error(astNodePtr, ErrorMessage, args.as.error);
    }

    ResultType(int, ErrorMessage) expectArrow = expectPeek(parser, CT_ARROW);
    if (expectArrow.error) {
        return Error(astNodePtr, ErrorMessage, expectArrow.as.error);
    }

    parserNextToken(parser);

    ResultType(astNodePtr, ErrorMessage) returnType = parseType(parser);
    if (returnType.error)
        return returnType;

    isInline = peekTokenIs(parser, CT_INLINE_FUNC_ARROW);

    if (!(isInline || peekTokenIs(parser, CT_OPEN_CURLY))) {
        Estr buffer = CREATE_ESTR("Expected next token to be '{' '=>', got \"");
        APPEND_ESTR(buffer, tokenTypeToCStr(parser->peekToken->type));
        APPEND_ESTR(buffer, "\" instead.");

        ErrorMessage errMsg = createError(
            parser->fileName,
            parser->sourceCode, 
            "InvalidSyntax",
            buffer.str,
            NULL,
            parser->peekToken->lineNum,
            parser->peekToken->startCol,
            parser->peekToken->endCol
        );

        return Error(astNodePtr, ErrorMessage, errMsg);
    }


    CometASTNode* block = NULL;    
    CometASTNode* inlineExpr = NULL;    
    if (!isInline) {
        ResultType(astNodePtr, ErrorMessage) blockResult = parseBlockStatement(parser);
        if (blockResult.error) {
            return blockResult;
        }

        block = blockResult.as.success;
    } else {
        parserNextToken(parser); // skip return type
        parserNextToken(parser); // skip the arrow
        ResultType(astNodePtr, ErrorMessage) exprResult = parseExpression(parser, PRECEDENCE_LOWEST);
        if (exprResult.error) {
            return exprResult;
        }

        inlineExpr = exprResult.as.success;
    }
    

    CometASTNode* stmt = AST_NODE(
        AST_FUNC_DEF_STATEMENT,
        lineNum,
        ident,
        block,
        args.as.success,
        returnType.as.success,
        isInline,
        inlineExpr,
        fieldAttrib
    );
    stmt->startCol = startCol;
    stmt->endCol = parser->currentToken->endCol;

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseFunctionCall(CometParser* parser, CometASTNode* left) {
    uint32_t lineNum = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    // 'left' is the already-parsed expression before the '('
    ResultType(argList, ErrorMessage) args = parseFunctionCallArgs(parser);
    if (args.error) {
        return Error(astNodePtr, ErrorMessage, args.as.error);
    }

    CometASTNode* funcCallNode = AST_NODE(AST_FUNC_CALL, lineNum, left, args.as.success);
    funcCallNode->startCol = startCol;
    funcCallNode->endCol = parser->currentToken->endCol;

    return Success(astNodePtr, ErrorMessage, funcCallNode);
}

ResultType(astNodePtr, ErrorMessage) parseReturnStatement(CometParser* parser) {
    uint32_t lineNum = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    parserNextToken(parser);
    ResultType(astNodePtr, ErrorMessage) expr = parseExpression(parser, PRECEDENCE_LOWEST);
    if (expr.error) {
        return expr;
    }

    CometASTNode* stmt = AST_NODE(AST_RETURN_STATEMENT, expr.as.success->lineNum, expr.as.success);
    stmt->startCol = startCol;
    stmt->endCol = expr.as.success->endCol;

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseConstructorDef(CometParser* parser) {
    uint32_t lineNum = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    parserNextToken(parser);

    ResultType(argList, ErrorMessage) constructorArgs = parseFunctionDefArgs(parser);
    if (constructorArgs.error)
        return Error(astNodePtr, ErrorMessage, constructorArgs.as.error);

    ResultType(astNodePtr, ErrorMessage) body = parseBlockStatement(parser);
    if (body.error)
        return body;

    CometASTNode* stmt = AST_NODE(AST_CONSTRUCTOR_DEF, lineNum, body.as.success, constructorArgs.as.success);
    stmt->startCol = startCol;
    stmt->endCol = body.as.success->endCol;

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseStructDefStatement(CometParser* parser) {
    // basic format:
    /*
    struct Animal {
        int age

        init() {
            self.age = 0
        }

        func speak() {
            print("...")
        }
    }*/

    uint32_t lineNum = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    ResultType(int, ErrorMessage) expectName = expectPeek(parser, CT_IDENT);
    if (expectName.error) {
        return Error(astNodePtr, ErrorMessage, expectName.as.error);
    }

    CometASTNode* structName = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, parser->currentToken->value.literal);
    structName->startCol = parser->currentToken->startCol;
    structName->endCol = parser->currentToken->endCol;

    CometASTNode* parentName = NULL;

    // the class inherits from something
    bool inherits = peekTokenIs(parser, CT_COLON);
    if (inherits) {
        parserNextToken(parser);

        ResultType(int, ErrorMessage) expectParentName = expectPeek(parser, CT_IDENT);
        if (expectParentName.error)
            return Error(astNodePtr, ErrorMessage, expectParentName.as.error);

        parentName = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, parser->currentToken->value.literal);
        parentName->startCol = parser->currentToken->startCol;
        parentName->endCol = parser->currentToken->endCol;
    }

    List(astNodePtr) fieldDefs = newList(astNodePtr);

    ResultType(astNodePtr, ErrorMessage) block = parseBlockStatement(parser);
    if (block.error) {
        return Error(astNodePtr, ErrorMessage, block.as.error);
    }

    struct AST_PROGRAM blockProgram = block.as.success->data.AST_PROGRAM;
    CometASTNode* constructor = NULL;

    for (size_t i = 0; i < blockProgram.numStatements; i++) {
        CometASTNode* statement = blockProgram.statements[i];

        switch (statement->nodeType) {
            case AST_ASSIGN_STATEMENT:
            case AST_FUNC_DEF_STATEMENT:
            case AST_OVERRIDE_STATEMENT: {
                append(fieldDefs, statement);
                break;
            }

            case AST_CONSTRUCTOR_DEF: {
                constructor = statement;
                break;
            }

            default: {
                Estr buffer = CREATE_ESTR("\"");
                APPEND_ESTR(buffer, ASTNodeTypeToCStr(statement->nodeType));
                APPEND_ESTR(buffer, "\" cannot be used in struct definition.");

                ErrorMessage errMsg = createError(
                    parser->fileName,
                    parser->sourceCode, 
                    "SemanticError",
                    buffer.str,
                    NULL,
                    statement->lineNum,
                    statement->startCol,
                    statement->endCol
                );

                return Error(astNodePtr, ErrorMessage, errMsg);
                break;
            }
        }
    }

    CometASTNode* stmt = AST_NODE(
        AST_STRUCT_DEF_STATEMENT,
        lineNum,
        structName,
        fieldDefs,
        constructor,
        NULL,
        parentName
    );
    stmt->startCol = startCol;
    stmt->endCol = parser->currentToken->endCol;

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseStructCreateStatement(CometParser* parser) {
    uint32_t lineNum = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    ResultType(int, ErrorMessage) expectName = expectPeek(parser, CT_IDENT);
    if (expectName.error)
        return Error(astNodePtr, ErrorMessage, expectName.as.error);

    CometASTNode* structName = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, parser->currentToken->value.literal);
    structName->startCol = parser->currentToken->startCol;
    structName->endCol = parser->currentToken->endCol;

    CometASTNode* structType = AST_NODE(AST_TYPE, structName->lineNum, structName, NULL, 0);
    structType->startCol = structName->startCol;
    structType->endCol = structName->endCol;

    parserNextToken(parser);

    ResultType(argList, ErrorMessage) constructorArgs = parseFunctionCallArgs(parser);
    if (constructorArgs.error)
        return Error(astNodePtr, ErrorMessage, constructorArgs.as.error);

    CometASTNode* stmt = AST_NODE(AST_NEW_STATEMENT, lineNum, structType, constructorArgs.as.success);
    stmt->startCol = startCol;
    stmt->endCol = parser->currentToken->endCol;

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseOverrideStatement(CometParser* parser, FieldAttribute fieldAttrib) {
    uint32_t lineNum = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    parserNextToken(parser); // skip "override"

    ResultType(astNodePtr, ErrorMessage) funcDef = parseFunctionDefStatement(parser, fieldAttrib);
    if (funcDef.error)
        return funcDef;

    CometASTNode* stmt = AST_NODE(AST_OVERRIDE_STATEMENT, lineNum, funcDef.as.success);
    stmt->startCol = startCol;
    stmt->endCol = funcDef.as.success->endCol;

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseImportStatement(CometParser* parser) {
    uint32_t lineNum = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    parserNextToken(parser); // skip "import"

    List(astNodePtr) importChain = newList(astNodePtr);

    while (true) {
        CometASTNode* importName = AST_NODE(AST_IDENTIFIER, parser->currentToken->lineNum, parser->currentToken->value.literal);
        importName->startCol = parser->currentToken->startCol;
        importName->endCol = parser->currentToken->endCol;

        append(importChain, importName);


        if (!peekTokenIs(parser, CT_DOT)) {
            break;
        }
        parserNextToken(parser);
        parserNextToken(parser);

        
    }

    CometASTNode* stmt = AST_NODE(AST_IMPORT_STATEMENT, lineNum, importChain);
    stmt->startCol = startCol;
    stmt->endCol = parser->currentToken->endCol;

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseBreakpointStatement(CometParser* parser) {
    CometASTNode* stmt = AST_NODE(AST_BREAKPOINT_STATEMENT, parser->currentToken->lineNum);
    stmt->startCol = parser->currentToken->startCol;
    stmt->endCol = parser->currentToken->endCol;
    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseTryStatement(CometParser* parser) {
    uint32_t lineNumber = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    ResultType(astNodePtr, ErrorMessage) tryBlock = parseOptionalBlockStatement(parser);
    if (tryBlock.error)
        return tryBlock;

    ResultType(int, ErrorMessage) expectExcept = expectPeekKeyword(parser, "except");
    if (expectExcept.error)
        return Error(astNodePtr, ErrorMessage, expectExcept.as.error);

    parserNextToken(parser);

    ResultType(astNodePtr, ErrorMessage) exceptionType = parseType(parser);
    if (exceptionType.error)
        return exceptionType;

    ResultType(astNodePtr, ErrorMessage) exceptBlock = parseOptionalBlockStatement(parser);
    if (tryBlock.error)
        return tryBlock;

    CometASTNode* stmt = AST_NODE(AST_TRY_STATEMENT, lineNumber, tryBlock.as.success, exceptBlock.as.success, exceptionType.as.success);
    stmt->startCol = startCol;
    stmt->endCol = parser->currentToken->endCol; 

    return Success(astNodePtr, ErrorMessage, stmt);
}
ResultType(astNodePtr, ErrorMessage) parseThrowStatement(CometParser* parser) {
    uint32_t lineNumber = parser->currentToken->lineNum;
    uint32_t startCol = parser->currentToken->startCol;

    parserNextToken(parser); // skip "throw"

    ResultType(astNodePtr, ErrorMessage) newStmt = parseExpression(parser, PRECEDENCE_LOWEST);
    if (newStmt.error)
        return newStmt;

    CometASTNode* stmt = AST_NODE(AST_THROW_STATEMENT, lineNumber, newStmt.as.success);
    stmt->startCol = startCol;
    stmt->endCol = parser->currentToken->endCol;

    return Success(astNodePtr, ErrorMessage, stmt);
}

ResultType(astNodePtr, ErrorMessage) parseKeyword(CometParser* parser, FieldAttribute fieldAttrib) {
    char* keyword = parser->currentToken->value.literal;

    if (strcmp(keyword, "while") == 0) {
        return parseWhileStatement(parser);
    } else if (strcmp(keyword, "for") == 0) {
        return parseForStatement(parser);
    } else if (strcmp(keyword, "if") == 0) {
        return parseIfStatement(parser);
    } else if (strcmp(keyword, "break") == 0) {
        return parseBreakStatement(parser);
    } else if (strcmp(keyword, "continue") == 0) {
        return parseContinueStatement(parser);
    } else if (strcmp(keyword, "func") == 0) {
        return parseFunctionDefStatement(parser, fieldAttrib);
    } else if (strcmp(keyword, "return") == 0) {
        return parseReturnStatement(parser);
    } else if (strcmp(keyword, "struct") == 0) {
        return parseStructDefStatement(parser);
    } else if (strcmp(keyword, "init") == 0) {
        return parseConstructorDef(parser);
    } else if (strcmp(keyword, "new") == 0) {
        return parseStructCreateStatement(parser);
    } else if (strcmp(keyword, "mut") == 0       ||
               strcmp(keyword, "public") == 0    ||
               strcmp(keyword, "private") == 0   ||
               strcmp(keyword, "readonly") == 0  ||
               strcmp(keyword, "protected") == 0   ) {
        parserNextToken(parser);
        return parseStatement(parser, strcmp(keyword, "mut") == 0, fieldAttribStringToAttribEnum(keyword));
    } else if (strcmp(keyword, "override") == 0) {
         return parseOverrideStatement(parser, fieldAttrib);
    } else if (strcmp(keyword, "import") == 0) {
        return parseImportStatement(parser);
    } else if (strcmp(keyword, "breakpoint") == 0) {
        return parseBreakpointStatement(parser);
    } else if (strcmp(keyword, "try") == 0) {
        return parseTryStatement(parser);
    } else if (strcmp(keyword, "throw") == 0) {
        return parseThrowStatement(parser);
    } else {
        char* buffer = malloc(128);
        sprintf(buffer, "Keyword \"%s\" was unexpected.", keyword);

        ErrorMessage errMsg = createError(
            parser->fileName,
            parser->sourceCode, 
            "InvalidSyntax",
            buffer,
            NULL,
            parser->currentToken->lineNum,
            parser->currentToken->startCol,
            parser->currentToken->endCol
        );

        return Error(astNodePtr, ErrorMessage, errMsg);
    }
}

// -- PARSER HELPERS -- //
ResultType(astNodePtr, ErrorMessage) parseStatement(CometParser* parser, bool isMutable, FieldAttribute fieldAttrib) {
    switch (parser->currentToken->type) {
        case CT_IDENT:
            return parseAssignmentStatement(parser, isMutable, fieldAttrib);

        case CT_KEYWORD:
            return parseKeyword(parser, fieldAttrib);

        default:
            return parseExpressionStatement(parser);
    }
}

// -- MAIN -- //
ResultType(astNodePtr, ErrorMessage) buildAST(CometParser* parser) {
    
    while (parser->currentToken->type != CT_EOF) {
        ResultType(astNodePtr, ErrorMessage) stmt = parseStatement(parser, false, FIELD_PUBLIC);
        if (stmt.error)
            return stmt;

        appendStatement(parser->program, stmt.as.success);
        parserNextToken(parser);
    }

    return Success(astNodePtr, ErrorMessage, parser->program);
}
