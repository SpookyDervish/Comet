#ifndef TYPE_H
#define TYPE_H

#include <stdint.h>
#include <stdbool.h>
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

    COMET_GENERIC
} CometValueTypeKind;

typedef struct CometStruct CometStruct;
typedef struct CometType CometType;
typedef struct CometFunction CometFunction;
typedef struct CometArrayType CometArrayType;

struct CometArrayType {
    CometType* elem; 
    bool isFixedSize[MAX_ARRAY_DEPTH];
    uint64_t fixedSize[MAX_ARRAY_DEPTH];
    uint8_t dims;
};

struct CometType {
    CometValueTypeKind typeKind;
    union {
        CometStruct* structType;
        CometFunction* functionType;
        CometArrayType* arrayType;
        char* genericParamName;
    };
};

typedef struct {
    char* genericTypeName;
    CometType newType;
} GenericTypeMapping;
UseList(GenericTypeMapping);

typedef struct CometMethod CometMethod;
struct CometMethod {
    char name[32];
    uint32_t blockIdx;
    uint32_t argCount;
    uint32_t symbolIdx;
    CometType returnType;
};

typedef enum {
    FUNC_FUNC,
    FUNC_METHOD
} CometFunctionType;

typedef struct {
    uint32_t pos;
    uint32_t blockPos;
    bool resolved;
} CometLabel;


extern CometType cometTypeSmall;
extern CometType cometTypeInt;
extern CometType cometTypeBig;
extern CometType cometTypeFloat;
extern CometType cometTypeDouble;
extern CometType cometTypeBool;
extern CometType cometTypeVoid;
extern CometType cometTypeString;

bool typesAreEqual(CometType a, CometType b);

#endif