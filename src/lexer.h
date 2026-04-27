#ifndef LEXER_H
#define LEXER_H

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/error.h"
#include "../include/estr.h"
#include "../include/list.h"
#include "token.h"

typedef CometToken* cometTokPtr;

UseList(CometToken);

typedef struct {
    char* source;
    size_t sourceLen;
    unsigned int pos;
    char currentChar;
    List(CometToken) tokens;
} CometLexer;

typedef List(CometToken) tokenList;

Result(CometLexer, charptr);
Result(CometToken, charptr);
Result(tokenList, charptr);
Result(Nothing, charptr);
Result(char, charptr);

bool isKeyword(char* string);
bool isBuiltInType(char* string);

ResultType(CometLexer, charptr) newLexer(char* source);
ResultType(tokenList, charptr) lex(CometLexer* lexer);
ResultType(Nothing, charptr) lexerConsume(CometLexer* lexer);
ResultType(char, charptr) lexerPeek(CometLexer* lexer);
ResultType(CometToken, charptr) lexerParseWord(CometLexer* lexer);
ResultType(CometToken, charptr) lexerParseNumber(CometLexer* lexer);
ResultType(CometToken, charptr) lexerParseString(CometLexer* lexer, char startingQuote);

extern const char* KEYWORDS[];
extern const char* BUILT_IN_TYPES[];

#endif