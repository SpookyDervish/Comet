#include "lexer.h"
#include "token.h"
#include <ctype.h>

const char* KEYWORDS[] = {
    "for",
    "while",
    "func",
    "struct",
    "private",
    "protected",
    "const",
    "readonly",
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
    "break",
    "continue"
    
};

const char* BUILT_IN_TYPES[] = {
    "int",
    "small",
    "big",
    "float",
    "double",
    "bool",
    "void",
};

#define TOKEN_LITERAL(tokType, tokValue) (CometToken){ .literalType = CL_STRING, .type = tokType, .value.literal = tokValue }

/*
Allocate a new lexer.

YOU MUST FREE THE LEXER WHEN YOU'RE DONE WITH IT.
*/
ResultType(CometLexer, charptr) newLexer(char* source) {
    CometLexer newLexer = {
        .source = source,
        .sourceLen = strlen(source),
        .pos = 0,
        .currentChar = source[0]
    };

    return Success(CometLexer, charptr, newLexer);
}

ResultType(Nothing, charptr) lexerConsume(CometLexer* lexer) {
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

bool isBuiltInType(char* buffer) {
    for (unsigned int i = 0; i < (sizeof(BUILT_IN_TYPES) / sizeof(BUILT_IN_TYPES[0])); i++) {
        if (strcmp(buffer, BUILT_IN_TYPES[i]) == 0) {
            return true;
        }
    }

    return false;
}

ResultType(CometToken, charptr) lexerParseWord(CometLexer* lexer) {
    CometToken tok;

    size_t bufferSize = 1;
    unsigned int bufferPos = 0;
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
                return Error(CometToken, charptr, "lexerParseWord: failed to allocate memory while increasing buffer size!");
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
    } else if (isBuiltInType(buffer)) {
        tok.type = CT_TYPE_NAME;
    } else {
        tok.type = CT_IDENT;
    }

    tok.value.literal = buffer;

    return Success(CometToken, charptr, tok);
}

ResultType(CometToken, charptr) lexerParseNumber(CometLexer* lexer) {
    CometToken tok;

    size_t bufferSize = 1;
    unsigned int bufferPos = 0;
    char* buffer = malloc(bufferSize);

    bool isFloat = false;

    while (lexer->pos < lexer->sourceLen) {
        char current = lexer->currentChar;

        if (current == '.') {

            if (isFloat) {
                free(buffer);
                return Error(CometToken, charptr, "Malformed number! (has multiple dots)");
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
                return Error(CometToken, charptr, "lexerParseNumber: failed to allocate memory while increasing buffer size!");
            }

            buffer = newPtr;
        }

        char peek = lexer->source[lexer->pos+1];
        if (isdigit(peek) && !isspace(peek)) {
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

    return Success(CometToken, charptr, tok);
}

ResultType(CometToken, charptr) lexerParseString(CometLexer* lexer, char startingQuote) {
    CometToken tok = {
        .literalType = CL_STRING,
        .type = CT_STRING_LITERAL
    };

    size_t bufferSize = 1;
    unsigned int bufferPos = 0;
    char* buffer = malloc(bufferSize);

    lexerConsume(lexer); // consume first quote

    while (lexer->pos < lexer->sourceLen) {
        char current = lexer->currentChar;

        if (current == startingQuote) {
            break;
        }

        buffer[bufferPos] = current;
        bufferPos++;

        if (bufferPos >= bufferSize) {
            bufferSize *= 2;
            char* newPtr = realloc(buffer, bufferSize);
            if (!newPtr) {
                free(buffer);
                return Error(CometToken, charptr, "lexerParseString: failed to allocate memory while increasing buffer size!");
            }

            buffer = newPtr;
        }

        lexerConsume(lexer);
    }
    buffer[bufferPos] = 0;

    lexerConsume(lexer); // consume last quote

    tok.value.literal = buffer;

    return Success(CometToken, charptr, tok);
}

ResultType(tokenList, charptr) lex(CometLexer* lexer) {
    List(CometToken) tokens = newList(CometToken);

    while (lexer->pos < lexer->sourceLen) {
        switch (lexer->currentChar) {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                break;
            case '=':
                ResultType(char, charptr) next = lexerPeek(lexer);

                if (next.error) {
                    append(tokens, TOKEN_LITERAL(CT_EQ, "="));
                    break;
                }
                lexerConsume(lexer);

                if (next.as.success == '=') {
                    
                    append(tokens, TOKEN_LITERAL(CT_EQ_EQ, "=="));
                 } else if (next.as.success == '>') {
                    append(tokens, TOKEN_LITERAL(CT_INLINE_FUNC_ARROW, "=>"))
                 } else {
                    if (isspace(next.as.success)) {
                        append(tokens, TOKEN_LITERAL(CT_EQ, "="))
                        break;
                    }  

                    return Error(tokenList, charptr, "Expected '=' or '>' after '='.");
                 }

                break; 
            case '{': append(tokens, TOKEN_LITERAL(CT_OPEN_CURLY, "{")); break;
            case '}': append(tokens, TOKEN_LITERAL(CT_CLOSE_CURLY, "}")); break;
            case '(': append(tokens, TOKEN_LITERAL(CT_OPEN_PAREN, "(")); break;
            case ')': append(tokens, TOKEN_LITERAL(CT_CLOSE_PAREN, ")")); break;
            case ':':

                ResultType(char, charptr) labelStart = lexerPeek(lexer);
                

                if (labelStart.error || !(isalnum(labelStart.as.success) || labelStart.as.success == '_')) {
                    append(tokens, TOKEN_LITERAL(CT_COLON, ":"));
                } else {
                    lexerConsume(lexer);
                    
                    ResultType(CometToken, charptr) endLabel = lexerParseWord(lexer);

                    if (endLabel.error) {
                        return Error(tokenList, charptr, endLabel.as.error);
                    }

                    

                    append(tokens, TOKEN_LITERAL(CT_END_LABEL, endLabel.as.success.value.literal));
                }

                break;
            case '.':
                ResultType(char, charptr) nextDot = lexerPeek(lexer);

                if (!nextDot.error && nextDot.as.success == '.') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_DOT_DOT, ".."));
                 } else {
                    append(tokens, TOKEN_LITERAL(CT_DOT, "."));
                 }

                break; 
            case '"':
            case '\'':
                
                ResultType(CometToken, charptr) stringTok = lexerParseString(lexer, lexer->currentChar);

                if (stringTok.error) {
                    return Error(tokenList, charptr, stringTok.as.error);
                }

                append(tokens, stringTok.as.success);

                break; 


            case '+': append(tokens, TOKEN_LITERAL(CT_PLUS, "+")); break;
            case '-':
                ResultType(char, charptr) arrow = lexerPeek(lexer);

                if (!arrow.error && arrow.as.success == '>') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_ARROW, "->"));
                 } else {
                    append(tokens, TOKEN_LITERAL(CT_MINUS, "-"));
                 }

                break; 
            case '*': append(tokens, TOKEN_LITERAL(CT_TIMES, "*")); break;
            case '/': append(tokens, TOKEN_LITERAL(CT_DIVIDE, "/")); break;
            case '%': append(tokens, TOKEN_LITERAL(CT_MOD, "%")); break;
            case '^': append(tokens, TOKEN_LITERAL(CT_POW, "^")); break;

            case ',': append(tokens, TOKEN_LITERAL(CT_COMMA, ",")) break;

            case '<':
                ResultType(char, charptr) ltEq = lexerPeek(lexer);

                if (!ltEq.error && ltEq.as.success == '=') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_LTE, "<="));
                 } else {
                    append(tokens, TOKEN_LITERAL(CT_LT, "<"));
                 }

                break; 
            case '>':
                ResultType(char, charptr) gtEq = lexerPeek(lexer);

                if (!gtEq.error && gtEq.as.success == '=') {
                    lexerConsume(lexer);
                    append(tokens, TOKEN_LITERAL(CT_GTE, ">="));
                 } else {
                    append(tokens, TOKEN_LITERAL(CT_GT, ">"));
                 }

                break; 
            
            default:
                if (isdigit(lexer->currentChar)) {
                    ResultType(CometToken, charptr) token = lexerParseNumber(lexer);

                    if (token.error) {
                        return Error(tokenList, charptr, token.as.error);
                    }

                    append(tokens, token.as.success);
                } else if (isalnum(lexer->currentChar)) {
                    ResultType(CometToken, charptr) token = lexerParseWord(lexer);

                    if (token.error) {
                        return Error(tokenList, charptr, token.as.error);
                    }

                    append(tokens, token.as.success);
                } else {
                    Estr buffer = CREATE_ESTR("Unexpected token \"");
                    char unexpected[] = {lexer->currentChar, 0};
                    APPEND_ESTR(buffer, unexpected);
                    APPEND_ESTR(buffer, "\"")

                    return Error(tokenList, charptr, buffer.str);
                }
        }

        lexerConsume(lexer);
    }

    return Success(tokenList, charptr, tokens);
}