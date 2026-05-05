#include "parser.h"
#include "ast.h"
#include "compiler.h"
#include "lexer.h"
#include "struct.h"
#include "token.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


ResultType(astNodePtr, charptr) parseStatement(CometParser* parser);

const CometTokenPrecedencePair PRECEDENCES[] = {
    {CT_PLUS, PRECEDENCE_SUM},
    {CT_MINUS, PRECEDENCE_SUM},
    {CT_TIMES, PRECEDENCE_PRODUCT},
    {CT_DIVIDE, PRECEDENCE_PRODUCT},
    {CT_MOD, PRECEDENCE_PRODUCT},
    {CT_POW, PRECEDENCE_EXPONENT},
    {CT_LT, PRECEDENCE_LESSGREATER},
    {CT_GT, PRECEDENCE_LESSGREATER},
    {CT_LTE, PRECEDENCE_LESSGREATER},
    {CT_GTE, PRECEDENCE_LESSGREATER},
    {CT_EQ_EQ, PRECEDENCE_EQUALS},
    {CT_NOT_EQ, PRECEDENCE_EQUALS},
    {CT_DOT, PRECEDENCE_INDEX},
    {CT_EQ, PRECEDENCE_SET}
};

ResultType(astNodePtr, charptr) parseIntLiteral(CometParser* parser);
ResultType(astNodePtr, charptr) parseIdentifier(CometParser* parser);
ResultType(astNodePtr, charptr) parseFloatLiteral(CometParser* parser);
ResultType(astNodePtr, charptr) parseBoolLiteral(CometParser* parser);
ResultType(astNodePtr, charptr) parseStringLiteral(CometParser* parser);
ResultType(astNodePtr, charptr) parseTypeName(CometParser* parser);
ResultType(astNodePtr, charptr) parseGroupedExpression(CometParser* parser);
ResultType(astNodePtr, charptr) parseReassignStatement(CometParser* parser);
ResultType(astNodePtr, charptr) parseStructCreateStatement(CometParser* parser);
const CometPrefixParseFn PREFIX_PARSE_FUNCTIONS[] = {
    {CT_INT_LITERAL, parseIntLiteral},
    {CT_FLOAT_LITERAL, parseFloatLiteral},
    {CT_STRING_LITERAL, parseStringLiteral},
    {CT_BOOL_LITERAL, parseBoolLiteral},
    {CT_IDENT, parseIdentifier},
    {CT_OPEN_PAREN, parseGroupedExpression},
    {CT_KEYWORD, parseStructCreateStatement}
};

ResultType(astNodePtr, charptr) parseInfixExpression(CometParser* parser, CometASTNode* left);
const CometInfixParseFn INFIX_PARSE_FUNCTIONS[] = {
    {CT_EQ, parseInfixExpression},
    {CT_PLUS, parseInfixExpression},
    {CT_MINUS, parseInfixExpression},
    {CT_TIMES, parseInfixExpression},
    {CT_DIVIDE, parseInfixExpression},
    {CT_MOD, parseInfixExpression},
    {CT_POW, parseInfixExpression},
    {CT_LT, parseInfixExpression},
    {CT_GT, parseInfixExpression},
    {CT_LTE, parseInfixExpression},
    {CT_GTE, parseInfixExpression},
    {CT_EQ_EQ, parseInfixExpression},
    {CT_NOT_EQ, parseInfixExpression},
    {CT_DOT, parseInfixExpression},
    {CT_EQ, parseInfixExpression}
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
        0,
        parser->statementArraySize
    );

    if (!parser->program->data.AST_PROGRAM.statements) {
        return Error(parserPtr, charptr, "newCometParser: Failed to allocate memory for program node statements!");
    }
    
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

ResultType(int, charptr) expectPeekKeyword(CometParser* parser, const char* keyword) {
    ResultType(int, charptr) next = expectPeek(parser, CT_KEYWORD);
    if (next.error) {
        return next;
    }

    

    if (strcmp(parser->currentToken->value.literal, keyword) != 0) {
        char* buffer = malloc(256);
        sprintf(buffer, "Expected next token to be %s but got %s instead.", keyword, parser->peekToken->value.literal);
        return Error(int, charptr, buffer);
    }

    return Success(int, charptr, 1);
}

bool peekKeywordIs(CometParser* parser, const char* keyword) {
    bool next = peekTokenIs(parser, CT_KEYWORD);
    if (!next) {
        return false;
    }
    return strcmp(parser->peekToken->value.literal, keyword) == 0;
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

        case AST_INT: printf("%lld", node->data.AST_INT.number); break;
        case AST_BOOL: printf("%s", node->data.AST_BOOL.value ? "true" : "false"); break;
        case AST_DOUBLE: printf("%f", node->data.AST_DOUBLE.number); break;
        case AST_STRING: printf("\"%s\"", node->data.AST_STRING.value); break;
        case AST_IDENTIFIER: printf("%s", node->data.AST_IDENTIFIER.ident); break;
            
        case AST_INFIX_EXPRESSION:
            printf("(");
            printNode(node->data.AST_INFIX_EXPRESSION.left);
            printf(" %s ", node->data.AST_INFIX_EXPRESSION.op.value.literal);
            printNode(node->data.AST_INFIX_EXPRESSION.right);
            printf(")");
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
        case AST_ASSIGN_STATEMENT:
            printNode(node->data.AST_ASSIGN_STATEMENT.type);
            printf(" ");
            printNode(node->data.AST_ASSIGN_STATEMENT.ident);

            CometASTNode* value = node->data.AST_ASSIGN_STATEMENT.expression;

            if (value) {
                printf(" = ");
                printNode(value);
            }
            break;
        case AST_REASSIGN_STATEMENT:
            printNode(node->data.AST_REASSIGN_STATEMENT.ident);
            printf(" = ");
            printNode(node->data.AST_REASSIGN_STATEMENT.expression);
            break;
        case AST_WHILE_STATEMENT:
            printf("while ");
            printNode(node->data.AST_WHILE_STATEMENT.expression);
            printf(" {\n");
            printNode(node->data.AST_WHILE_STATEMENT.program);
            printf("       } :while");
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
                printf("       } :");
                printNode(node->data.AST_FUNC_DEF_STATEMENT.ident);
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

            printf(":if");
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
            printf("       } :for");
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

        default:
            printf("reached unkown node type (got %d)\n", node->nodeType);
            break;
        
    }
}

// -- EXPRESSION METHODS -- //
ResultType(astNodePtr, charptr) parseExpression(CometParser* parser, CometPrecedenceType precedence) {

    ResultType(prefixFuncType, charptr) prefixFunc = getPrefixFunc(parser->currentToken->type);

    if (prefixFunc.error) {
        char* buffer = malloc(128);
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
    if (parser->currentToken->type == CT_EQ) {
        parserNextToken(parser);

        ResultType(astNodePtr, charptr) right = parseExpression(parser, PRECEDENCE_LOWEST);

        CometASTNode* assign = AST_NODE(AST_REASSIGN_STATEMENT, left, right.as.success);
        return Success(astNodePtr, charptr, assign);
    }

    CometASTNode* infixExpr = AST_NODE(AST_INFIX_EXPRESSION, left, NULL, *parser->currentToken);

    CometPrecedenceType precedence = currentPrecedence(parser);
    parserNextToken(parser);

    ResultType(astNodePtr, charptr) right = parseExpression(parser, precedence);
    if (right.error)
        return right;

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

ResultType(argList, charptr) parseFunctionDefArgs(CometParser* parser) {
    List(astNodePtr) args = newList(astNodePtr);

    while (parser->currentToken->type != CT_CLOSE_PAREN) {
        if (parser->peekToken->type == CT_EOF) {
            return Error(argList, charptr, "Function args were not closed!");
        } else if (parser->peekToken->type == CT_CLOSE_PAREN) {
            parserNextToken(parser);
            break;
        }

        

        ResultType(int, charptr) expectType = expectPeek(parser, CT_IDENT);
        if (expectType.error) {
            return Error(argList, charptr, expectType.as.error);
        }
        CometASTNode* type = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

        ResultType(int, charptr) expectArgName = expectPeek(parser, CT_IDENT);
        if (expectArgName.error) {
            return Error(argList, charptr, expectArgName.as.error);
        }
        CometASTNode* argName = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

        append(args, AST_NODE(AST_ARG_DEF, type, argName));

        ResultType(int, charptr) expectComma = expectPeek(parser, CT_COMMA);
        if (expectComma.error && !peekTokenIs(parser, CT_CLOSE_PAREN)) {
            return Error(argList, charptr, expectComma.as.error);
        }
    }

    return Success(argList, charptr, args);
}

ResultType(argList, charptr) parseFunctionCallArgs(CometParser* parser) {
    List(astNodePtr) args = newList(astNodePtr);

    parserNextToken(parser); // skip open paren
    while (parser->currentToken->type != CT_CLOSE_PAREN) {
        if (parser->peekToken->type == CT_EOF) {
            return Error(argList, charptr, "Function args were not closed!");
        }

        

        ResultType(astNodePtr, charptr) expr = parseExpression(parser, PRECEDENCE_LOWEST);
        if (expr.error) {
            return Error(argList, charptr, expr.as.error);
        }

        append(args, expr.as.success);

        ResultType(int, charptr) expectComma = expectPeek(parser, CT_COMMA);
        if (expectComma.error && !peekTokenIs(parser, CT_CLOSE_PAREN)) {
            return Error(argList, charptr, expectComma.as.error);
        }

        parserNextToken(parser); // skip comma
    }

    return Success(argList, charptr, args);
}

// -- PREFIX METHODS -- //
ResultType(astNodePtr, charptr) parseIntLiteral(CometParser* parser) {
    return Success(astNodePtr, charptr, AST_NODE(AST_INT, parser->currentToken->value.intVal));
}

ResultType(astNodePtr, charptr) parseFloatLiteral(CometParser* parser) {
    return Success(astNodePtr, charptr, AST_NODE(AST_DOUBLE, parser->currentToken->value.doubleVal));
}

ResultType(astNodePtr, charptr) parseBoolLiteral(CometParser* parser) {
    return Success(astNodePtr, charptr, AST_NODE(AST_BOOL, parser->currentToken->value.boolVal));
}

ResultType(astNodePtr, charptr) parseStringLiteral(CometParser* parser) {
    return Success(astNodePtr, charptr, AST_NODE(AST_STRING, parser->currentToken->value.literal));
}

ResultType(astNodePtr, charptr) parseIdentifier(CometParser* parser) {
    CometASTNode* ident = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

    
    if (peekTokenIs(parser, CT_OPEN_PAREN)) {
        parserNextToken(parser);
        

        ResultType(argList, charptr) args = parseFunctionCallArgs(parser);
        if (args.error) {
            return Error(astNodePtr, charptr, args.as.error);
        }

        CometASTNode* funcCallNode = AST_NODE(AST_FUNC_CALL, ident, args.as.success);
        return Success(astNodePtr, charptr, funcCallNode);
    }

    return Success(astNodePtr, charptr, ident);
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

ResultType(astNodePtr, charptr) parseAssignmentStatement(CometParser* parser, bool isMutable, FieldAttribute fieldAttrib) {
    // basic format:
    // small myVar = 10

    if (currentTokenIs(parser, CT_KEYWORD)) {
        char* keyword = parser->currentToken->value.literal;

        if (strcmp(keyword, "mut") == 0) {
            if (isMutable) {
                return Error(astNodePtr, charptr, "\"mut\" keyword appears twice in variable declaration.");
            }
            
            parserNextToken(parser);
            return parseAssignmentStatement(parser, true, fieldAttrib);
        } else if (
            strcmp(keyword, "private") == 0 ||
            strcmp(keyword, "protected") == 0 ||
            strcmp(keyword, "readonly") == 0) {
            if (fieldAttrib != FIELD_PUBLIC) {
                return Error(astNodePtr, charptr, "Cannot set multiple attributes to struct field.");
            }

            parserNextToken(parser);
            return parseAssignmentStatement(parser, isMutable, attribStringToFieldAttrib(keyword));
        }
    }

    CometASTNode* type = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

    bool expectName = peekTokenIs(parser, CT_IDENT);
    if (!expectName)
        return parseExpression(parser, PRECEDENCE_LOWEST);
    parserNextToken(parser);

    CometASTNode* ident = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

    CometASTNode* stmt = AST_NODE(AST_ASSIGN_STATEMENT, ident, NULL, type, isMutable, fieldAttrib);

    bool hasValue = peekTokenIs(parser, CT_EQ);
    

    if (!hasValue) {
        return Success(astNodePtr, charptr, stmt);
    }

    parserNextToken(parser);
    parserNextToken(parser);
    

    ResultType(astNodePtr, charptr) value = parseExpression(parser, PRECEDENCE_LOWEST);
    if (value.error) {
        return value;
    }

    stmt->data.AST_ASSIGN_STATEMENT.expression = value.as.success;
    
    return Success(astNodePtr, charptr, stmt);
}

ResultType(astNodePtr, charptr) parseBlockStatement(CometParser* parser) {
    CometASTNode** statements = calloc(1, sizeof(CometASTNode*));
    if (!statements) {
        return Error(astNodePtr, charptr, "parseBlockStatement: failed to allocate memory for block statement!");
    }

    CometASTNode* program = AST_NODE(AST_PROGRAM, statements, 0, 1);

    ResultType(int, charptr) openCurly = expectPeek(parser, CT_OPEN_CURLY);
    if (openCurly.error) {
        return Error(astNodePtr, charptr, openCurly.as.error);
    }

    parserNextToken(parser);

    while (parser->currentToken->type != CT_CLOSE_CURLY) {
        if (parser->currentToken->type == CT_EOF) {
            return Error(astNodePtr, charptr, "Block statement was not closed!");
        }

        ResultType(astNodePtr, charptr) stmt = parseStatement(parser);
        if (stmt.error)
            return stmt;

        appendStatement(program, stmt.as.success);

        parserNextToken(parser);
        
    }

    return Success(astNodePtr, charptr, program);
}

ResultType(astNodePtr, charptr) parseOptionalBlockStatement(CometParser* parser) {
    // used for stuff like
    // if x + 1 == 2
    //     break

    CometASTNode* stmt;
    if (!peekTokenIs(parser, CT_OPEN_CURLY)) { // if 1+1 == 2 return

        parserNextToken(parser);
        ResultType(astNodePtr, charptr) innerStmt = parseStatement(parser);

        if (innerStmt.error)
            return innerStmt;

        CometASTNode** statements = calloc(1, sizeof(CometASTNode*));
        if (!statements) {
            return Error(astNodePtr, charptr, "parseOptionalBlockStatement: failed to allocate memory for block statement!");
        }

        CometASTNode* program = AST_NODE(AST_PROGRAM, statements, 0, 1);
        appendStatement(program, innerStmt.as.success);

        stmt = program;
    } else {                                             // if 1+1 == 2 { return }
        
        ResultType(astNodePtr, charptr) block = parseBlockStatement(parser);
        if (block.error) {
            return block;
        }

        
        stmt = block.as.success;
        
    }

    return Success(astNodePtr, charptr, stmt);
}

ResultType(astNodePtr, charptr) parseWhileStatement(CometParser* parser) {
    // basic format:
    // while true {}

    parserNextToken(parser);

    ResultType(astNodePtr, charptr) expression = parseExpression(parser, PRECEDENCE_LOWEST);
    if (expression.error) {
        return expression;
    }

    ResultType(astNodePtr, charptr) program = parseOptionalBlockStatement(parser);
    if (program.error) {
        return program;
    }

    CometASTNode* stmt = AST_NODE(AST_WHILE_STATEMENT, expression.as.success, program.as.success);

    return Success(astNodePtr, charptr, stmt);
}

ResultType(astNodePtr, charptr) parseForStatement(CometParser* parser) {
    // basic format
    // for int i in 0 .. 10 {}

    ResultType(int, charptr) expectType = expectPeek(parser, CT_IDENT);
    if (expectType.error) {
        return Error(astNodePtr, charptr, expectType.as.error);
    }

    CometASTNode* type = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

    ResultType(int, charptr) expectIdent = expectPeek(parser, CT_IDENT);
    if (expectIdent.error) {
        return Error(astNodePtr, charptr, expectIdent.as.error);
    }

    CometASTNode* ident = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

    

    ResultType(int, charptr) expectIn = expectPeekKeyword(parser, "in");
    if (expectIn.error) {
        return Error(astNodePtr, charptr, expectIn.as.error);
    }

    parserNextToken(parser);

    ResultType(astNodePtr, charptr) start = parseExpression(parser, PRECEDENCE_LOWEST);
    if (start.error) {
        return start;
    }

    ResultType(int, charptr) dotDot = expectPeek(parser, CT_DOT_DOT);
    if (dotDot.error) {
        return Error(astNodePtr, charptr, dotDot.as.error);
    }

    parserNextToken(parser);

    ResultType(astNodePtr, charptr) end = parseExpression(parser, PRECEDENCE_LOWEST);
    if (end.error) {
        return end;
    }

    ResultType(astNodePtr, charptr) stepNode;
    ResultType(int, charptr) step = expectPeekKeyword(parser, "step");
   
    if (!step.error) {
        parserNextToken(parser);
        stepNode = parseExpression(parser, PRECEDENCE_LOWEST);
        if (stepNode.error) {
            return stepNode;
        }
    } else {
        stepNode = Success(astNodePtr, charptr, AST_NODE(AST_INT, 1));
    }

    ResultType(astNodePtr, charptr) block = parseOptionalBlockStatement(parser);
    if (block.error) {
        return block;
    }

    CometASTNode* stmt = AST_NODE(
        AST_FOR_STATEMENT,
        type,
        ident,
        start.as.success,
        end.as.success,
        stepNode.as.success,
        block.as.success
    );
    return Success(astNodePtr, charptr, stmt);
}

ResultType(astNodePtr, charptr) parseBreakStatement(CometParser* parser) {
    return Success(astNodePtr, charptr, AST_NODE(AST_BREAK_STATEMENT));
}

ResultType(astNodePtr, charptr) parseContinueStatement(CometParser* parser) {
    return Success(astNodePtr, charptr, AST_NODE(AST_CONTINUE_STATEMENT));
}

ResultType(astNodePtr, charptr) parseIfStatement(CometParser* parser) {
    // basic format
    // if 1+1 == 2 {}

    parserNextToken(parser);

    ResultType(astNodePtr, charptr) expr = parseExpression(parser, PRECEDENCE_LOWEST);
    if (expr.error) {
        return expr;
    }
    
    ResultType(astNodePtr, charptr) body = parseOptionalBlockStatement(parser);
    if (body.error)
        return body;

    CometASTNode* elseBody = NULL;

    bool elseKeyword = peekKeywordIs(parser, "else");

    if (elseKeyword) {
        parserNextToken(parser);
        ResultType(astNodePtr, charptr) elseResult = parseOptionalBlockStatement(parser);
        if (elseResult.error)
            return elseResult;

        elseBody = elseResult.as.success;
    }

    CometASTNode* stmt = AST_NODE(
        AST_IF_STATEMENT,
        expr.as.success,
        body.as.success,
        elseBody
    );
     
    


    return Success(astNodePtr, charptr, stmt);
}

ResultType(astNodePtr, charptr) parseReassignStatement(CometParser* parser) {
    // basic format
    // x = x + 1

    /*CometASTNode* ident = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

    if (peekTokenIs(parser, CT_OPEN_PAREN)) { // function call
        return parseIdentifier(parser);
    }

    ResultType(int, charptr) expectEq = expectPeek(parser, CT_EQ);
    if (expectEq.error) {
        return Error(astNodePtr, charptr, expectEq.as.error);
    }

    parserNextToken(parser);

    ResultType(astNodePtr, charptr) expr = parseExpression(parser, PRECEDENCE_LOWEST);
    if (expr.error) {
        return expr;
    }

    CometASTNode* stmt = AST_NODE(AST_REASSIGN_STATEMENT, ident, expr.as.success);
    return Success(astNodePtr, charptr, stmt);*/
    return parseExpressionStatement(parser);
}

ResultType(astNodePtr, charptr) parseFunctionDefStatement(CometParser* parser) {
    // basic format
    // func func_name(type arg_name, type2 arg_name2) -> return_type {
    //     return arg_name + arg_name2
    // }
    //
    // inline body func
    // func func_name(type arg_name, type2 arg_name2) => arg_name + arg_name2

    ResultType(int, charptr) expectName = expectPeek(parser, CT_IDENT);
    if (expectName.error) {
        return Error(astNodePtr, charptr, expectName.as.error);
    }

    CometASTNode* ident = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);
    bool isInline = false;

    ResultType(int, charptr) expectOpenParen = expectPeek(parser, CT_OPEN_PAREN);
    if (expectOpenParen.error) {
        return Error(astNodePtr, charptr, expectOpenParen.as.error);
    }

    // parse args
    ResultType(argList, charptr) args = parseFunctionDefArgs(parser);
    if (args.error) {
        return Error(astNodePtr, charptr, args.as.error);
    }

    ResultType(int, charptr) expectArrow = expectPeek(parser, CT_ARROW);
    if (expectArrow.error) {
        return Error(astNodePtr, charptr, expectArrow.as.error);
    }

    ResultType(int, charptr) expectReturnType = expectPeek(parser, CT_IDENT);
    if (expectReturnType.error) {
        return Error(astNodePtr, charptr, expectReturnType.as.error);
    }

    CometASTNode* returnType = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

    isInline = peekTokenIs(parser, CT_INLINE_FUNC_ARROW);

    if (!(isInline || peekTokenIs(parser, CT_OPEN_CURLY))) {
        return Error(astNodePtr, charptr, "Expected next token to be '{' '=>'.");
    }

    CometASTNode* block = NULL;    
    CometASTNode* inlineExpr = NULL;    
    if (!isInline) {
        ResultType(astNodePtr, charptr) blockResult = parseBlockStatement(parser);
        if (blockResult.error) {
            return blockResult;
        }

        block = blockResult.as.success;
    } else {
        parserNextToken(parser); // skip return type
        parserNextToken(parser); // skip the arrow
        ResultType(astNodePtr, charptr) exprResult = parseExpression(parser, PRECEDENCE_LOWEST);
        if (exprResult.error) {
            return exprResult;
        }

        inlineExpr = exprResult.as.success;
    }
    

    CometASTNode* stmt = AST_NODE(
        AST_FUNC_DEF_STATEMENT,
        ident,
        block,
        args.as.success,
        returnType,
        isInline,
        inlineExpr
    );
    return Success(astNodePtr, charptr, stmt);
}

ResultType(astNodePtr, charptr) parseReturnStatement(CometParser* parser) {
    

    parserNextToken(parser);
    ResultType(astNodePtr, charptr) expr = parseExpression(parser, PRECEDENCE_LOWEST);
    if (expr.error) {
        return expr;
    }

    return Success(astNodePtr, charptr, AST_NODE(AST_RETURN_STATEMENT, expr.as.success));
}

ResultType(astNodePtr, charptr) parseConstructorDef(CometParser* parser) {
    parserNextToken(parser);

    ResultType(argList, charptr) constructorArgs = parseFunctionDefArgs(parser);
    if (constructorArgs.error)
        return Error(astNodePtr, charptr, constructorArgs.as.error);

    ResultType(astNodePtr, charptr) body = parseBlockStatement(parser);
    if (body.error)
        return body;

    CometASTNode* stmt = AST_NODE(AST_CONSTRUCTOR_DEF, body.as.success, constructorArgs.as.success);

    return Success(astNodePtr, charptr, stmt);
}

ResultType(astNodePtr, charptr) parseStructDefStatement(CometParser* parser) {
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

    ResultType(int, charptr) expectName = expectPeek(parser, CT_IDENT);
    if (expectName.error) {
        return Error(astNodePtr, charptr, expectName.as.error);
    }

    CometASTNode* structName = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

    List(astNodePtr) fieldDefs = newList(astNodePtr);

    ResultType(astNodePtr, charptr) block = parseBlockStatement(parser);
    if (block.error) {
        return Error(astNodePtr, charptr, block.as.error);
    }

    struct AST_PROGRAM blockProgram = block.as.success->data.AST_PROGRAM;
    CometASTNode* constructor = NULL;

    for (size_t i = 0; i < blockProgram.numStatements; i++) {
        CometASTNode* statement = blockProgram.statements[i];

        switch (statement->nodeType) {
            case AST_ASSIGN_STATEMENT: {
                append(fieldDefs, statement);
                break;
            }

            case AST_FUNC_DEF_STATEMENT: {
                append(fieldDefs, statement);
                break;
            }

            case AST_CONSTRUCTOR_DEF: {
                constructor = statement;
                break;
            }

            default: {
                Estr errMsg = CREATE_ESTR("\"");
                APPEND_ESTR(errMsg, ASTNodeTypeToCStr(statement->nodeType));
                APPEND_ESTR(errMsg, "\" cannot be used in struct definition.");
                return Error(astNodePtr, charptr, errMsg.str);
                break;
            }
        }
    }

    CometASTNode* stmt = AST_NODE(
        AST_STRUCT_DEF_STATEMENT,
        structName,
        fieldDefs,
        constructor,
        NULL
    );
    return Success(astNodePtr, charptr, stmt);
}

ResultType(astNodePtr, charptr) parseStructCreateStatement(CometParser* parser) {
    ResultType(int, charptr) expectName = expectPeek(parser, CT_IDENT);
    if (expectName.error)
        return Error(astNodePtr, charptr, expectName.as.error);

    CometASTNode* structName = AST_NODE(AST_IDENTIFIER, parser->currentToken->value.literal);

    parserNextToken(parser);

    ResultType(argList, charptr) constructorArgs = parseFunctionCallArgs(parser);
    if (constructorArgs.error)
        return Error(astNodePtr, charptr, constructorArgs.as.error);

    CometASTNode* stmt = AST_NODE(AST_NEW_STATEMENT, structName, constructorArgs.as.success);

    return Success(astNodePtr, charptr, stmt);
}

ResultType(astNodePtr, charptr) parseKeyword(CometParser* parser) {
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
        return parseFunctionDefStatement(parser);
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
        return parseAssignmentStatement(parser, false, FIELD_PUBLIC);
    
    } else {
        char* buffer = malloc(128);
        sprintf(buffer, "No parse method for keyword \"%s\"", keyword);
        return Error(astNodePtr, charptr, buffer);
    }
}

// -- PARSER HELPERS -- //
ResultType(astNodePtr, charptr) parseStatement(CometParser* parser) {
    switch (parser->currentToken->type) {
        case CT_IDENT:
            return parseAssignmentStatement(parser, false, FIELD_PUBLIC);

        case CT_KEYWORD:
            return parseKeyword(parser);

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

        appendStatement(parser->program, stmt.as.success);
        parserNextToken(parser);
    }

    return Success(astNodePtr, charptr, parser->program);
}
