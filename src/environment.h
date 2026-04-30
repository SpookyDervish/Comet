#pragma once
#include "ast.h"
#include "lexer.h"
#include "token.h"
#include <llvm-c/Types.h>
#include <uthash.h>
#include <stdlib.h>
#include <stdio.h>

// basically the same as a token but with a name instead of a type
typedef struct {
    char* name;
    UT_hash_handle hh;
    LLVMTypeRef type;
    LLVMValueRef ptr;
} Record;

typedef struct CometEnvironment CometEnvironment;
struct CometEnvironment {
    CometEnvironment* parent;
    char* name;
    Record* records;
};


CometEnvironment* newEnvironment(char* name, CometEnvironment* parent);
void defineVar(CometEnvironment* env, char* name, LLVMValueRef ptr, LLVMTypeRef type);
Record* lookup(CometEnvironment* env, char* name);