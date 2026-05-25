#pragma once
#include "ast.h"
#include "lexer.h"
#include "operand.h"
#include "token.h"
#include <uthash.h>
#include <stdlib.h>
#include <stdio.h>

// basically the same as a token but with a name instead of a type
typedef struct {
    char* name;
    UT_hash_handle hh;
    bool isMutable;
    CometOperand value;
    uint32_t recordIdx;
} Record;

typedef struct CometEnvironment CometEnvironment;
struct CometEnvironment {
    CometEnvironment* parent;
    char* name;
    uint32_t recordIdx;
    Record* records;
};


CometEnvironment* newEnvironment(char* name, CometEnvironment* parent);
uint32_t defineVar(CometEnvironment* env, char* name, CometOperand value, bool isMutable);
Record* lookup(CometEnvironment* env, char* name);