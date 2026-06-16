#ifndef LEXER_H
#define LEXER_H

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/error.h"
#include "../lib/estr.h"
#include "../include/list.h"
#include "token.h"
#include "error_message.h"

typedef CometToken* cometTokPtr;

UseList(CometToken);

typedef struct {
    char* filePath;
    char* source;
    size_t sourceLen;

    uint32_t pos;
    uint32_t lineNum;
    uint32_t column;

    char currentChar;
    List(CometToken) tokens;
} CometLexer;

typedef List(CometToken) tokenList;

Result(CometToken, ErrorMessage);
Result(tokenList, ErrorMessage);
Result(Nothing, charptr);
Result(char, charptr);

bool isKeyword(char* string);
bool isBuiltInType(char* string);

CometLexer newLexer(char* source, char* filePath);
ResultType(tokenList, ErrorMessage) lex(CometLexer* lexer);
ResultType(Nothing, charptr) lexerConsume(CometLexer* lexer);
ResultType(char, charptr) lexerPeek(CometLexer* lexer);
ResultType(CometToken, ErrorMessage) lexerParseWord(CometLexer* lexer);
ResultType(CometToken, ErrorMessage) lexerParseNumber(CometLexer* lexer);
ResultType(CometToken, ErrorMessage) lexerParseString(CometLexer* lexer, char startingQuote);

extern const char* KEYWORDS[];

#endif