#ifndef COMETLIB_H
#define COMETLIB_H

#include <stdint.h>
#include <stdbool.h>
#include "vm.h"
#include "function.h"

#if defined(_WIN32) || defined(__CYGWIN__)
    #define API_EXPORT __declspec(dllexport)
#else
    #ifdef __GNUC__
        #define API_EXPORT __attribute__((visibility("default")))
    #else
        #define API_EXPORT
    #endif
#endif

static CometType cometTypeSmall  = (CometType){.typeKind = COMET_SMALL };
static CometType cometTypeInt    = (CometType){.typeKind = COMET_INT   };
static CometType cometTypeBig    = (CometType){.typeKind = COMET_BIG   };
static CometType cometTypeFloat  = (CometType){.typeKind = COMET_FLOAT };
static CometType cometTypeDouble = (CometType){.typeKind = COMET_DOUBLE};
static CometType cometTypeBool   = (CometType){.typeKind = COMET_BOOL  };
static CometType cometTypeVoid   = (CometType){.typeKind = COMET_VOID  };

static CometArrayType stringArray = {
    .elem = &cometTypeSmall,
    .isFixedSize = {false},
    .dims = 1
};

static CometType cometTypeString = {
    .typeKind = COMET_ARRAY,
    .arrayType = &stringArray
};

#define on_import void onImport(CometEnvironment* env)

typedef CometFunction* cometFuncPtr;

typedef struct {
    char* name;
    CometType type;
} StructField;

UseList(CometSerializedFunc);
UseList(cometFuncPtr);
UseList(charptr);
UseList(StructField);



CometType createArrayType(CometType elem, uint8_t dimensions, bool isFixedSize[], uint64_t fixedSize[]);

API_EXPORT CometSerializedFunc cometSerializeFunction(
    CometVM* vm,
    CometFunction* func,
    externalLibFunc funcPtr
);

API_EXPORT CometOperand cometCreateObject(CometSerializedStruct* structType);

API_EXPORT CometOperand cometValue(CometValueTypeKind valueType, ...);

API_EXPORT int64_t serializeValue(CometOperand value);
API_EXPORT CometOperand deserializeValue(int64_t value, CometType type);
API_EXPORT void* cometArrayToCArray(CometOperand arrayValue, CometType elemType);
API_EXPORT CometOperand CArrayToCometArray(void* arrayValue, size_t length, CometType elemType);

API_EXPORT CometFunction* cometDefineFunc(
    CometEnvironment* env,
    char* name,
    CometType returnType,
    uint32_t numArgs,
    bool isVarArgs,
    ...
);
API_EXPORT CometFunction* cometDefineMethod(
    CometEnvironment* env,
    char* name,
    CometStruct* cometStruct,
    CometType returnType,
    uint32_t numArgs,
    bool isVarArgs,
    ...
);

API_EXPORT void setStructFieldsAndMethods(CometStruct* cometStruct, List(StructField) fields, List(cometFuncPtr) methods);

API_EXPORT CometStruct* cometDefineStruct(CometEnvironment* env, char* name);
API_EXPORT void cometDefineConstructor(
    CometEnvironment* env,
    CometStruct* structType,
    uint32_t numArgs,
    bool isVarArgs,
    ...
);
API_EXPORT void cometSetField(CometObject* object, uint32_t index, int64_t value);

#endif