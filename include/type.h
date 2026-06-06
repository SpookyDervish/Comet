#ifndef TYPE_H
#define TYPE_H

#include <stdint.h>

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
    COMET_MODULE,   // not actually represented in assembly but this is used so
                    // we can resolve imported values
} CometValueTypeKind;

typedef struct CometType CometType;
typedef struct CometStruct CometStruct;
typedef struct CometFunction CometFunction;

struct CometType {
    CometValueTypeKind typeKind;
    union {
        CometStruct* structType;
        CometFunction* functionType;
    };
};

struct CometFunction {
    char name[32];
    uint32_t startIdx;
    uint32_t argCount;
    CometType returnType;
    bool isMethod;
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

// the actual struct type
struct CometStruct {
    CometMethod** vtable;
    uint32_t numMethods;
    uint32_t fieldCount;
    char** fieldNames;
    CometType* fieldTypes;
    char* name;
};

typedef struct {
    uint32_t pos;
    bool resolved;
} CometLabel;



#endif