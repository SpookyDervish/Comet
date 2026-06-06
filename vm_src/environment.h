#pragma once
#include "ast.h"
#include "lexer.h"
#include "../include/comet_operand.h"
#include "token.h"
#include <uthash.h>
#include <stdlib.h>
#include <stdio.h>

typedef enum {
    RECORD_LOCAL,
    RECORD_ARG
} RecordType;

// basically the same as a token but with a name instead of a type
typedef struct {
    char* name;
    UT_hash_handle hh;
    bool isMutable;
    CometOperand value;
    CometType type;
    uint32_t recordIdx;
    RecordType recordType;
} Record;

typedef struct CometEnvironment CometEnvironment;
struct CometEnvironment {
    CometEnvironment* parent;
    char* name;
    uint32_t recordIdx;
    uint32_t argIdx;
    Record* records;
};


CometEnvironment* newEnvironment(char* name, CometEnvironment* parent, bool isFunction);
uint32_t defineVar(CometEnvironment* env, char* name, RecordType recordType, CometOperand value, CometType type, bool isMutable);
Record* lookup(CometEnvironment* env, char* name);
CometEnvironment* destroyEnv(CometEnvironment* env);