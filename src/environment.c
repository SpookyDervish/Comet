#include "environment.h"
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>


CometEnvironment* newEnvironment(char* name, CometEnvironment* parent, bool isFunction) {
    // parent can be NULL if its the root environment

    CometEnvironment* env = (CometEnvironment*)malloc(sizeof(CometEnvironment));
    env->name = name;
    env->parent = parent;
    env->records = NULL;

    env->recordIdx = env->parent ? parent->recordIdx : 0;
    env->argIdx = isFunction ? 0 : env->parent ? parent->argIdx : 0;

    return env;
}

void increaseRecordIndex(CometEnvironment* env) {
    env->recordIdx++;
    if (env->parent) {
        increaseRecordIndex(env->parent);
    }
}

uint32_t defineVar(CometEnvironment* env, char* name, RecordType recordType, CometOperand value, CometType type, bool isMutable) {
    Record* record;
    HASH_FIND_STR(env->records, name, record);

    // avoid duplication of keys
    if (record != NULL) {
        printf("Redeclaration of %s!\n", name);
        return 0;
    }

    uint32_t idx = recordType == RECORD_LOCAL ? env->recordIdx : env->argIdx;

    // create a new record and save it to the hash map (or dictionary or whatever you wanna call it)
    record = malloc(sizeof(Record));
    record->name = strdup(name);
    record->value = value;
    record->type = type;
    record->isMutable = isMutable;
    record->recordType = recordType;

    printf("env name: %s\n", env->name);

    record->recordIdx = idx;
    if (recordType == RECORD_ARG) {
        printf("defining arg %s at index: %d\n", name, idx);
        env->argIdx++;
    } else {
        printf("defining var %s at index: %d\n", name, idx);
        increaseRecordIndex(env); // avoid index collisions
    }

    

    HASH_ADD_KEYPTR(hh, env->records, record->name, strlen(record->name), record);

    return record->recordIdx;
}

CometEnvironment* destroyEnv(CometEnvironment* env) {
    CometEnvironment* parent = env->parent;

    parent->recordIdx -= env->recordIdx;
    free(env);

    return parent;
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