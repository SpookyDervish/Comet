#pragma once
#include "ast.h"
#include "lexer.h"
#include "token.h"
#include <uthash.h>

// basically the same as a token but with a name instead of a type
typedef struct {
    char* name;
    UT_hash_handle hh;
    char* type;
    CometASTNode value;
} Record;

typedef struct CometEnvironment CometEnvironment;
struct CometEnvironment {
    CometEnvironment* parent;
    char* name;
    Record* records;
};


CometEnvironment* newEnvironment(char* name, CometEnvironment* parent);
void defineVar(CometEnvironment* env, char* name, CometASTNode value, char* type);
Record* lookup(CometEnvironment* env, char* name);