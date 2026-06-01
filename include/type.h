#ifndef TYPE_H
#define TYPE_H

#include <stdint.h>

typedef struct {
    char name[32];
    uint32_t startIdx;
    uint32_t argCount;
} CometFunction;

typedef struct {
    uint32_t startIdx;
    uint32_t argCount;
    uint32_t symbolIdx;
} CometMethod;

// the actual struct type
typedef struct CometType CometType;
typedef struct {
    CometMethod** vtable;
    uint32_t numMethods;
    uint32_t fieldCount;
    char** fieldNames;
    CometType* fieldTypes;
    char* name;
} CometStruct;

typedef struct {
    uint32_t pos;
    bool resolved;
} CometLabel;

typedef enum {
    COMET_VOID,
    COMET_SMALL,
    COMET_INT,
    COMET_BIG,
    COMET_FLOAT,
    COMET_DOUBLE,
    COMET_BOOL,
    COMET_STRUCT,
    COMET_FUNCTION
} CometValueTypeKind;

struct CometType {
    CometValueTypeKind typeKind;
    union {
        CometStruct* structType;
        CometFunction* functionType;
    };
};

#endif