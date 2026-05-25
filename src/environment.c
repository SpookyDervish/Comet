#include "environment.h"


CometEnvironment* newEnvironment(char* name, CometEnvironment* parent) {
    // parent can be NULL if its the root environment

    CometEnvironment* env = (CometEnvironment*)malloc(sizeof(CometEnvironment));
    env->name = name;
    env->parent = parent;
    env->records = NULL;
    env->recordIdx = 0;
    return env;
}

uint32_t defineVar(CometEnvironment* env, char* name, CometOperand value, bool isMutable) {
    Record* record;
    HASH_FIND_STR(env->records, name, record);

    // avoid duplication of keys
    if (record != NULL) {
        printf("Redeclaration of %s!\n", name);
        return 0;
    }

    // create a new record and save it to the hash map (or dictionary or whatever you wanna call it)
    record = malloc(sizeof(Record));
    record->name = strdup(name);
    record->value = value;
    record->isMutable = isMutable;

    record->recordIdx = env->recordIdx;
    env->recordIdx++;

    HASH_ADD_KEYPTR(hh, env->records, record->name, strlen(record->name), record);

    return record->recordIdx;
}

Record* lookup(CometEnvironment* env, char* name) {
    // search for the record and if it doesn't exist check for it in the parent scope
    Record* record;
    HASH_FIND_STR(env->records, name, record);

    if (record != NULL) {
        return record;
    }

    if (env->parent) {
        return lookup(env->parent, name);
    }

    return NULL;
}