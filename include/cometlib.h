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

UseList(CometSerializedFunc);

API_EXPORT void cometDefineFunc(
    CometEnvironment* env,
    char* name,
    CometType returnType,
    uint32_t numArgs,
    ...
);
API_EXPORT CometSerializedFunc* cometDefineMethod(
    CometEnvironment* env,
    char* name,
    CometType returnType,
    uint32_t numArgs,
    ...
);

API_EXPORT CometSerializedStruct* cometCreateStruct(List(CometSerializedFunc) methods, uint32_t numFields);
API_EXPORT CometOperand cometCreateObject(CometSerializedStruct* structType);

API_EXPORT CometOperand cometValue(CometValueTypeKind valueType, ...);

#endif