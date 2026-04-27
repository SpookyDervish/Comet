#ifndef TOKEN_H
#define TOKEN_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum {
    // words
    CT_IDENT, CT_KEYWORD, CT_TYPE_NAME,

    // literals
    CT_INT_LITERAL, CT_FLOAT_LITERAL, CT_BOOL_LITERAL, CT_STRING_LITERAL,

    // symbols
    CT_EQ_EQ, CT_LT, CT_GT, CT_LTE, CT_GTE,
    CT_COLON,
    CT_EQ,
    CT_DOT, CT_DOT_DOT, CT_ARROW,
    CT_OPEN_CURLY, CT_CLOSE_CURLY, CT_OPEN_PAREN, CT_CLOSE_PAREN,

    // other
    CT_COMMENT, CT_END_LABEL
} CometTokenType;

typedef enum {
    CL_STRING,
    CL_INT,
    CL_DOUBLE,
    CL_BOOL
} CometLiteralType;

typedef struct {
    CometTokenType type;
    CometLiteralType literalType;
    union {
        char* literal;
        int64_t intVal;
        double doubleVal;
        bool boolVal;
    } value;
} CometToken;

char* tokenTypeToCStr(CometTokenType tokType);
char* tokenToCStr(CometToken tok);

#endif