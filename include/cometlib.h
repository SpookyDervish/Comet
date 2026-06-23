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

#define cometTypeSmall (CometType){.typeKind = COMET_SMALL}
#define cometTypeInt (CometType){.typeKind = COMET_INT}
#define cometTypeBig (CometType){.typeKind = COMET_BIG}
#define cometTypeFloat (CometType){.typeKind = COMET_FLOAT}
#define cometTypeDouble (CometType){.typeKind = COMET_DOUBLE}
#define cometTypeBool (CometType){.typeKind = COMET_BOOL}
#define cometTypeVoid (CometType){.typeKind = COMET_VOID}

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

API_EXPORT CometFunction* cometDefineFunc(
    CometEnvironment* env,
    char* name,
    CometType returnType,
    uint32_t numArgs,
    bool isVarArgs,
    bool isMethod,
    ...
);

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

API_EXPORT CometStruct* cometDefineStruct(CometEnvironment* env, char* name, List(StructField) fields, List(cometFuncPtr) methods);
API_EXPORT void cometDefineConstructor(
    CometEnvironment* env,
    CometStruct* structType,
    uint32_t numArgs,
    bool isVarArgs,
    ...
);
API_EXPORT void cometSetField(CometObject* object, uint32_t index, int64_t value);

#endif