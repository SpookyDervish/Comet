#include "constant_env.h"

ConstantEnv* newConstantEnv(ConstantEnv* parent, char* envName) {
    ConstantEnv* newEnv = (ConstantEnv*)malloc(sizeof(ConstantEnv));
    newEnv->parent = parent;
    newEnv->records = NULL;
    newEnv->name = envName;

    return newEnv;
}

void defineConstant(ConstantEnv* env, char* name, CometASTNode* value) {
    ConstantRecord* record = findConstantLocal(env, name);

    if (record) {
        record->value = value;
        return;
    }

    record = malloc(sizeof(ConstantRecord));
    record->name = strdup(name);
    record->value = value;

    HASH_ADD_KEYPTR(hh, env->records, record->name, strlen(record->name), record);
}

ConstantRecord* findConstantLocal(ConstantEnv* env, char* name) {
    ConstantRecord* record;
    HASH_FIND_STR(env->records, name, record);
    return record;
}

ConstantRecord* findConstant(ConstantEnv* env, char* name) {
    ConstantRecord* record;
    HASH_FIND_STR(env->records, name, record);

    if (record != NULL) {
        return record;
    }

    if (env->parent) {
        return findConstant(env->parent, name);
    }

    return NULL;
}

void removeConstant(ConstantEnv* env, char* name) {
    ConstantRecord* record;
    HASH_FIND_STR(env->records, name, record);

    if (record) {
        record->value = NULL;
        return;
    }

    record = malloc(sizeof(ConstantRecord));
    record->name = strdup(name);
    record->value = NULL;

    HASH_ADD_KEYPTR(hh, env->records, record->name, strlen(record->name), record);
}

ConstantEnv* destroyConstantEnv(ConstantEnv* env) {
    ConstantEnv* parent = env->parent;
    free(env);
    return parent;
}