#include "lexer.h"
#include "token.h"
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

const char* KEYWORDS[] = {
    "for",
    "while",
    "func",
    "struct",
    "mut",
    "private",
    "protected",
    "const",
    "readonly",
    "override",
    "init",
    "in",
    "return",
    "new",
    "can",
    "be",
    "enum",
    "import",
    "step",
    "if",
    "else",
    "break",
    "continue",
    "new",
    "breakpoint",
    "try",
    "except",
    "throw"
};

#define TOKEN_LITERAL(tokType, tokValue, lexer) (CometToken){ .literalType = CL_STRING, .type = tokType, .value.literal = tokValue, .lineNum = lexer->lineNum, .startCol = lexer->column - strlen(tokValue), .endCol = lexer->column }
#define TOKEN_CHAR(tokType, tokValue, lexer) (CometToken){ .literalType = CL_STRING, .type = tokType, .value.literal = tokValue, .lineNum = lexer->lineNum, .startCol = lexer->column, .endCol = lexer->column }


CometLexer newLexer(char* source, char* filePath) {
    return (CometLexer){
        .source = source,
        .sourceLen = strlen(source),
        .pos = 0,
        .currentChar = source[0],
        .filePath = filePath,
        .lineNum = 1,
        .column = 1
    };
}

ResultType(Nothing, charptr) lexerConsume(CometLexer* lexer) {
    if (lexer->currentChar == '\n') {
        lexer->column = 1;
        lexer->lineNum++;
    } else {
        lexer->column++;
    }

    lexer->pos++;

    if (lexer->pos >= lexer->sourceLen) {
        return Error(Nothing, charptr, "Went past <EOF> of source!");
    }

    lexer->currentChar = lexer->source[lexer->pos];
    

    return Success(Nothing, charptr, {});
}

ResultType(char, charptr) lexerPeek(CometLexer* lexer) {
    if (lexer->pos + 1 >= lexer->sourceLen) {
        return Error(char, charptr, "Went past <EOF> of source!");
    }

    return Success(char, charptr, lexer->source[lexer->pos + 1]);
}

bool isKeyword(char* buffer) {
    for (unsigned int i = 0; i < (sizeof(KEYWORDS) / sizeof(KEYWORDS[0])); i++) {
        if (strcmp(buffer, KEYWORDS[i]) == 0) {
            return true;
        }
    }

    return false;
}

bool isBoolean(char* buffer) {
    if (strcmp(buffer, "true") == 0 || strcmp(buffer, "false") == 0) {
        return true;
    }

    return false;
}

CometToken booleanToken(char* buffer) {
    return (CometToken){
        .literalType = CL_BOOL,
        .value.boolVal = strcmp(buffer, "true") == 0,
        .type = CT_BOOL_LITERAL
    };
}

ResultType(CometToken, ErrorMessage) lexerParseWord(CometLexer* lexer) {
    CometToken tok = {
        .startCol = lexer->column,
        .lineNum = lexer->lineNum
    };

    size_t bufferSize = 1;

    uint32_t startColumn = lexer->column;

    uint32_t bufferPos = 0;
    char* buffer = malloc(bufferSize);

    while (lexer->pos < lexer->sourceLen) {
        char current = lexer->currentChar;

        if (isspace(current) || !(isalnum(current) || current == '_')) {
            break;
        }

        buffer[bufferPos] = current;
        bufferPos++;

        if (bufferPos >= bufferSize) {
            bufferSize *= 2;
            char* newPtr = realloc(buffer, bufferSize);
            if (!newPtr) {
                free(buffer);

                ErrorMessage errMsg = createError(
                    lexer->filePath,
                    lexer->source,
                    "MemoryAllocFail",
                    "lexerParseWord: failed to allocate memory while increasing buffer size",
                    NULL,
                    lexer->lineNum,
                    startColumn,
                    lexer->column
                );

                return Error(CometToken, ErrorMessage, errMsg);
            }

            buffer = newPtr;
        }

        char peek = lexer->source[lexer->pos+1];
        if (isspace(peek) || !(isalnum(peek) || peek == '_')) {
            break;
        } else {
            lexerConsume(lexer);
        }
        
    }
    buffer[bufferPos] = 0;

    tok.literalType = CL_STRING;

    if (isKeyword(buffer)) {
        tok.type = CT_KEYWORD;
    } else if (isBoolean(buffer)) {

        CometToken boolTok = booleanToken(buffer);
        free(buffer);
        return Success(CometToken, ErrorMessage, boolTok);

    } else {
        tok.type = CT_IDENT;
    }

    tok.value.literal = buffer;

    tok.endCol = lexer->column;

    return Success(CometToken, ErrorMessage, tok);
}

ResultType(CometToken, ErrorMessage) lexerParseNumber(CometLexer* lexer) {
    CometToken tok = {
        .startCol = lexer->column,
        .lineNum = lexer->lineNum
    };

    size_t bufferSize = 1;

    uint32_t startColumn = lexer->column;

    uint32_t bufferPos = 0;
    char* buffer = malloc(bufferSize);

    bool isNegative = false;
    bool isFloat = false;


    while (lexer->pos < lexer->sourceLen) {
        char current = lexer->currentChar;

        if (current == '-') {
            if (isNegative) {
                free(buffer);

                ErrorMessage errMsg = createError(
                    lexer->filePath,
                    lexer->source,
                    "InvalidSyntax",
                    "Malformed number! (has multiple negative signs)",
                    NULL,
                    lexer->lineNum,
                    startColumn,
                    lexer->column
                );

                return Error(CometToken, ErrorMessage, errMsg);
            }

            isNegative = true;
        } else if (current == '.') {

            if (isFloat) {
                free(buffer);

                ErrorMessage errMsg = createError(
                    lexer->filePath,
                    lexer->source,
                    "InvalidSyntax",
                    "Malformed number! (has multiple dots)",
                    NULL,
                    lexer->lineNum,
                    startColumn,
                    lexer->column
                );

                return Error(CometToken, ErrorMessage, errMsg);
            }

            isFloat = true;

        } else {

            if (isspace(current) || !isdigit(current)) {
                break;
            }

        }

        buffer[bufferPos] = current;
        bufferPos++;

        if (bufferPos >= bufferSize) {
            bufferSize *= 2;
            char* newPtr = realloc(buffer, bufferSize);
            if (!newPtr) {
                free(buffer);

                ErrorMessage errMsg = createError(
                    lexer->filePath,
                    lexer->source,
                    "MemoryAllocFail",
                    "lexerParseNumber: failed to allocate memory while increasing buffer size",
                    NULL,
                    lexer->lineNum,
                    startColumn,
                    lexer->column
                );

                return Error(CometToken, ErrorMessage, errMsg);
            }

            buffer = newPtr;
        }

        char peek = lexer->source[lexer->pos+1];
        if ((isdigit(peek) && !isspace(peek)) || peek == '.') {
            lexerConsume(lexer);
        } else {
            break;
        }
    }
    buffer[bufferPos] = 0;

    if (isFloat) {
        tok.type = CT_FLOAT_LITERAL;
        tok.literalType = CL_DOUBLE;

        tok.value.doubleVal = strtod(buffer, NULL);
    } else {
        tok.type = CT_INT_LITERAL;
        tok.literalType = CL_INT;

        tok.value.intVal = strtoll(buffer, NULL, 10);
    }

    tok.endCol = lexer->column;

    return Success(CometToken, ErrorMessage, tok);
}

ResultType(CometToken, ErrorMessage) lexerParseString(CometLexer* lexer, char startingQuote) {
    CometToken tok = {
        .literalType = CL_STRING,
        .type = CT_STRING_LITERAL,
        .startCol = lexer->column,
        .lineNum = lexer->lineNum
    };

    uint32_t startColumn = lexer->column;

    size_t bufferSize = 1;
    uint32_t bufferPos = 0;
    char* buffer = malloc(bufferSize);

    lexerConsume(lexer);

    while (lexer->currentChar != startingQuote && lexer->pos < lexer->sourceLen) {
        char current = lexer->currentChar;

        buffer[bufferPos] = current;
        bufferPos++;

        if (bufferPos >= bufferSize) {
            bufferSize *= 2;
            char* newPtr = realloc(buffer, bufferSize);
            if (!newPtr) {
                free(buffer);
                ErrorMessage errMsg = createError(
                    lexer->filePath,
                    lexer->source,
                    "MemoryAllocFail",
                    "lexerParseString: failed to allocate memory while increasing buffer size",
                    NULL,
                    lexer->lineNum,
                    startColumn,
                    lexer->column
                );

                return Error(CometToken, ErrorMessage, errMsg);
            }

            buffer = newPtr;
        }

        char peek = lexer->source[lexer->pos+1];

        lexerConsume(lexer);

        if (peek == startingQuote) {
            break;
        }

        
    }
    buffer[bufferPos] = 0;

    tok.value.literal = buffer;
    tok.endCol = lexer->column;

    return Success(CometToken, ErrorMessage, tok);
}

ResultType(tokenList, ErrorMessage) lex(CometLexer* lexer) {
    List(CometToken) tokens = newList(CometToken);

    while (lexer->pos < lexer->sourceLen) {
        switch (lexer->currentChar) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                break;
            case '!': {
                ResultType(char, charptr) next = lexerPeek(lexer);

                if (next.error || next.as.success != '=') {
                    append(tokens, TOKEN_CHAR(CT_NOT, "!", lexer));
                    break;
                }
                lexerConsume(lexer);

                if (next.as.success == '=') {
                    
                    append(tokens, TOKEN_LITERAL(CT_NOT_EQ, "!=", lexer));
                } else {
                    if (isspace(next.as.success)) {
                        append(tokens, TOKEN_LITERAL(CT_NOT, "!", lexer))
                        break;
                    }  

                    ErrorMessage errMsg = createError(
                        lexer->filePath,
                        lexer->source,
                        "InvalidSyntax",
                        "Expected '=' after '!'",
                        NULL,
                        lexer->lineNum,
                        lexer->column,
                        lexer->column
                    );

                    return Error(tokenList, ErrorMessage, errMsg);
                }

                break;
            }
            case '=': {
                ResultType(char, charptr) next = lexerPeek(lexer);

                if (next.error) {
                    append(tokens, TOKEN_CHAR(CT_EQ, "=", lexer));
                    break;
                }
                lexerConsume(lexer);

                if (next.as.success == '=') {
                    
                    append(tokens, TOKEN_LITERAL(CT_EQ_EQ, "==", lexer));
                 } else if (next.as.success == '>') {
                    append(tokens, TOKEN_LITERAL(CT_INLINE_FUNC_ARROW, "=>", lexer))
                 } else {
                    if (isspace(next.as.success)) {
                        append(tokens, TOKEN_CHAR(CT_EQ, "=", lexer))
                        break;
                    }  

                    ErrorMessage errMsg = createError(
                        lexer->filePath,
                        lexer->source,
                        "InvalidSyntax",
                        "Expected '=' or '>' after '='",
                        NULL,
                        lexer->lineNum,
                        lexer->column,
                        lexer->column
                    );

                    return Error(tokenList, ErrorMessage, errMsg);
                 }

                break; 
            }
            case '{': append(tokens, TOKEN_CHAR(CT_OPEN_CURLY, "{", lexer)); break;
            case '}': append(tokens, TOKEN_CHAR(CT_CLOSE_CURLY, "}", lexer)); break;
            case '(': append(tokens, TOKEN_CHAR(CT_OPEN_PAREN, "(", lexer)); break;
            case ')': append(tokens, TOKEN_CHAR(CT_CLOSE_PAREN, ")", lexer)); break;
            case '[': append(tokens, TOKEN_CHAR(CT_OPEN_SQUARE, "[", lexer)); break;
            case ']': append(tokens, TOKEN_CHAR(CT_CLOSE_SQUARE, "]", lexer)); break;
            case '#': append(tokens, TOKEN_CHAR(CT_HASH, "#", lexer)); break;
            case ':': append(tokens, TOKEN_CHAR(CT_COLON, ":", lexer)); break;
            
            case '.': {
                ResultType(char, charptr) nextDot = lexerPeek(lexer);

                if (!nextDot.error && nextDot.as.success == '.') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_DOT_DOT, "..", lexer));
                 } else {
                    append(tokens, TOKEN_CHAR(CT_DOT, ".", lexer));
                 }

                break; 
            }
            case '\"':
            case '\'': {
                
                ResultType(CometToken, ErrorMessage) stringTok = lexerParseString(lexer, lexer->currentChar);

                if (stringTok.error) {
                    return Error(tokenList, ErrorMessage, stringTok.as.error);
                }

                append(tokens, stringTok.as.success);

                break; 
            }

            case '+': {
                ResultType(char, charptr) eq = lexerPeek(lexer);

                if (!eq.error && eq.as.success == '=') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_PLUS_EQ, "+=", lexer));
                    break;
                }

                append(tokens, TOKEN_CHAR(CT_PLUS, "+", lexer));
                break;
            }
            case '-': {
                ResultType(char, charptr) arrow = lexerPeek(lexer);

                if (!arrow.error && arrow.as.success == '>') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_ARROW, "->", lexer));
                } else if (!arrow.error && arrow.as.success == '=') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_MINUS_EQ, "-=", lexer));
                } else if (isdigit(arrow.as.success)) {

                    ResultType(CometToken, ErrorMessage) numberTok = lexerParseNumber(lexer);
                    if (numberTok.error)
                        return Error(tokenList, ErrorMessage, numberTok.as.error);

                    append(tokens, numberTok.as.success);
                 } else {
                    append(tokens, TOKEN_CHAR(CT_MINUS, "-", lexer));
                 }

                break; 
            }
            case '*': {
                ResultType(char, charptr) eq = lexerPeek(lexer);

                if (!eq.error && eq.as.success == '=') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_TIMES_EQ, "*=", lexer));
                    break;
                }

                append(tokens, TOKEN_CHAR(CT_TIMES, "*", lexer));
                break;
            }
            case '/': {
                ResultType(char, charptr) eq = lexerPeek(lexer);

                if (!eq.error && eq.as.success == '=') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_DIVIDE_EQ, "/=", lexer));
                    break;
                }

                append(tokens, TOKEN_CHAR(CT_DIVIDE, "/", lexer));
                break;
            }
            case '%': {
                ResultType(char, charptr) eq = lexerPeek(lexer);

                if (!eq.error && eq.as.success == '=') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_MOD_EQ, "%=", lexer));
                    break;
                }

                append(tokens, TOKEN_CHAR(CT_MOD, "%", lexer));
                break;
            }
            case '^': {
                ResultType(char, charptr) eq = lexerPeek(lexer);

                if (!eq.error && eq.as.success == '=') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_POW_EQ, "^=", lexer));
                    break;
                }

                append(tokens, TOKEN_CHAR(CT_POW_EQ, "^", lexer));
                break;
            }

            case ',': append(tokens, TOKEN_CHAR(CT_COMMA, ",", lexer)) break;

            case '<': {
                ResultType(char, charptr) ltEq = lexerPeek(lexer);

                if (!ltEq.error && ltEq.as.success == '=') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_LTE, "<=", lexer));
                 } else {
                    append(tokens, TOKEN_CHAR(CT_LT, "<", lexer));
                 }

                break; 
            }
            case '>': {
                ResultType(char, charptr) gtEq = lexerPeek(lexer);

                if (!gtEq.error && gtEq.as.success == '=') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_GTE, ">=", lexer));
                 } else {
                    append(tokens, TOKEN_CHAR(CT_GT, ">", lexer));
                 }

                break; 
            }
            
            default: {
                if (isdigit(lexer->currentChar)) {
                    ResultType(CometToken, ErrorMessage) token = lexerParseNumber(lexer);

                    if (token.error) {
                        return Error(tokenList, ErrorMessage, token.as.error);
                    }

                    append(tokens, token.as.success);
                } else if (isalnum(lexer->currentChar)) {
                    ResultType(CometToken, ErrorMessage) token = lexerParseWord(lexer);

                    if (token.error) {
                        return Error(tokenList, ErrorMessage, token.as.error);
                    }

                    append(tokens, token.as.success);
                } else {
                    Estr buffer = CREATE_ESTR("Unexpected token \"");
                    char unexpected[] = {lexer->currentChar, 0};
                    APPEND_ESTR(buffer, unexpected);
                    APPEND_ESTR(buffer, "\"")

                    ErrorMessage errMsg = createError(
                        lexer->filePath,
                        lexer->source,
                        "InvalidSyntax",
                        buffer.str,
                        NULL,
                        lexer->lineNum,
                        lexer->column,
                        lexer->column
                    ); 

                    return Error(tokenList, ErrorMessage, errMsg);
                }
            }
        }

        lexerConsume(lexer);
    }

    return Success(tokenList, ErrorMessage, tokens);
}