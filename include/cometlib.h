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

#define on_import void onImport(CometEnvironment* env)

typedef CometFunction* cometFuncPtr;

typedef struct {
    char* name;
    CometType type;
} StructField;

UseList(CometSerializedFunc);
UseList(cometFuncPtr);
UseList(StructField);

API_EXPORT CometStruct* cometGetExceptionStruct(CometEnvironment* env);

CometType cometCreateArrayType(CometType elem, uint8_t dimensions, bool isFixedSize[], uint64_t fixedSize[]);

API_EXPORT CometSerializedFunc cometSerializeFunction(
    CometVM* vm,
    CometFunction* func,
    externalLibFunc funcPtr
);

API_EXPORT CometObject* cometCreateObject(CometSerializedStruct* structType);

API_EXPORT CometOperand cometValue(CometValueTypeKind valueType, ...);

API_EXPORT int8_t cometArgSmall(int64_t argVal);
API_EXPORT int32_t cometArgInt(int64_t argVal);
API_EXPORT int64_t cometArgBig(int64_t argVal);
API_EXPORT float cometArgFloat(int64_t argVal);
API_EXPORT double cometArgDouble(int64_t argVal);
API_EXPORT bool cometArgBool(int64_t argVal);
API_EXPORT char* cometArgString(int64_t argVal);
API_EXPORT uintptr_t cometArgPointer(int64_t argVal);
API_EXPORT int64_t cometSerializeString(char* cString);

API_EXPORT CometType cometGenericType(char* name);

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

API_EXPORT StructField cometCreateField(char* name, CometType type);

API_EXPORT void cometSetStructFieldsAndMethods(CometStruct* cometStruct, List(StructField) fields, List(cometFuncPtr) methods);

API_EXPORT CometStruct* cometDefineStruct(CometEnvironment* env, char* name, CometStruct* parent);
API_EXPORT CometStruct* cometDefineGenericStruct(CometEnvironment* env, char* name, CometStruct* parent, List(charptr) genericTypeNames);

API_EXPORT void cometDefineConstructor(
    CometEnvironment* env,
    CometStruct* structType,
    uint32_t numArgs,
    bool isVarArgs,
    ...
);
API_EXPORT void cometSetField(CometObject* object, uint32_t index, int64_t value);

API_EXPORT CometSerializedStruct* cometVMGetStruct(CometVM* vm, char* structName);

API_EXPORT ResultType(int64_t, objectPtr) cometError(CometVM* vm, char* errorName, char* errorMessage);

#endif