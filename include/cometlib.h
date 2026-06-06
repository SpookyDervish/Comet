#ifndef COMETLIB_H
#define COMETLIB_H

#include "comet_operand.h"
#include "serialized.h"
#include <stdint.h>
#include <stdbool.h>
#include "../lib/list.h"
#include "vm.h"

#define cometTypeSmall (CometType){.typeKind = COMET_SMALL}
#define cometTypeInt (CometType){.typeKind = COMET_INT}
#define cometTypeBig (CometType){.typeKind = COMET_BIG}
#define cometTypeFloat (CometType){.typeKind = COMET_FLOAT}
#define cometTypeDouble (CometType){.typeKind = COMET_DOUBLE}
#define cometTypeBool (CometType){.typeKind = COMET_BOOL}
#define cometTypeVoid (CometType){.typeKind = COMET_VOID}

#define on_import void onImport(CometVM* vm, CometEnvironment* env)

UseList(CometSerializedFunc);
UseList(CometOperand);

void cometDefineFunc(
    CometEnvironment* env,
    char* name,
    CometOperand (*funcPtr)(List(CometOperand) args, CometVM* vm),
    CometType returnType,
    uint32_t numArgs,
    ...
);
CometSerializedFunc* cometDefineMethod(
    CometEnvironment* env,
    char* name,
    CometOperand (*funcPtr)(List(CometOperand) args, CometVM* vm),
    CometType returnType,
    uint32_t numArgs,
    ...
);

CometSerializedStruct* cometCreateStruct(List(CometSerializedFunc) methods, uint32_t numFields);
CometOperand cometCreateObject(CometSerializedStruct* structType);

CometOperand cometValue(CometValueTypeKind valueType, ...);

#endif