/*

A stripped down version of CometEnvironment used for optimisation passes.

*/

#ifndef CONSTANT_ENV_H
#define CONSTANT_ENV_H

#include "ast.h"
#include <stdlib.h>
#include <uthash.h>

typedef struct {
    char* name;
    CometASTNode* value;
    UT_hash_handle hh;
} ConstantRecord;

typedef struct ConstantEnv ConstantEnv;
struct ConstantEnv {
    ConstantRecord* records;
    ConstantEnv* parent;
    char* name;
};

ConstantEnv* newConstantEnv(ConstantEnv* parent, char* envName);
void defineConstant(ConstantEnv* env, char* name, CometASTNode* value);
ConstantRecord* findConstant(ConstantEnv* env, char* name);
ConstantRecord* findConstantLocal(ConstantEnv* env, char* name);
void removeConstant(ConstantEnv* env, char* name);
ConstantEnv* destroyConstantEnv(ConstantEnv* env);

#endif