#ifndef TYPE_H
#define TYPE_H

#include <stdint.h>
#include "list.h"

#define MAX_ARRAY_DEPTH 8

typedef enum {
    COMET_VOID,
    COMET_SMALL,
    COMET_INT,
    COMET_BIG,
    COMET_FLOAT,
    COMET_DOUBLE,
    COMET_BOOL,
    COMET_STRUCT,
    COMET_FUNCTION,
    COMET_ARRAY,
    COMET_MODULE,   // not actually represented in assembly but this is used so
                    // we can resolve imported values

    COMET_TYPE,     // this is also not actually represented in the asm
} CometValueTypeKind;

typedef struct CometStruct CometStruct;
typedef struct CometType CometType;
typedef struct CometFunction CometFunction;
typedef struct CometArrayType CometArrayType;

struct CometArrayType {
    CometType* elem; 
    bool isFixedSize[MAX_ARRAY_DEPTH];
    uint64_t fixedSize[MAX_ARRAY_DEPTH];
};



struct CometType {
    CometValueTypeKind typeKind;
    union {
        CometStruct* structType;
        CometFunction* functionType;
        CometArrayType* arrayType;
    };
};

typedef struct {
    char name[32];
    uint32_t startIdx;
    uint32_t argCount;
    uint32_t symbolIdx;
    CometType returnType;
} CometMethod;

typedef enum {
    FUNC_FUNC,
    FUNC_METHOD
} CometFunctionType;

typedef struct {
    uint32_t pos;
    bool resolved;
} CometLabel;



#endif