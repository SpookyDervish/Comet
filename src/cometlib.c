#include "../include/cometlib.h"
#include "../include/environment.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void cometDefineFunc(
    CometEnvironment* env,
    char* name,
    CometOperand (*funcPtr)(List(CometOperand) args, CometVM* vm),
    CometType returnType,
    uint32_t numArgs,
    ...
) {
    CometFunction* func = malloc(sizeof(CometFunction));
    memcpy(func->name, name, 32);
    func->argCount = numArgs;
    func->isMethod = false;
    func->returnType = returnType;
    func->startIdx = 0;
    func->isExternal = true;
    func->externalPtr = funcPtr;
    
    CometType type = {
        .typeKind = COMET_FUNCTION,
    };
    type.functionType = func;

    CometOperand funcVal = {
        .type = CO_IMMEDIATE,
        .imm = {
            .typeKind = COMET_FUNCTION,
            .bigVal = (int64_t)func
        }
    };

    defineVar(env, name, RECORD_LOCAL, funcVal, type, false);
}

CometSerializedStruct* cometCreateStruct(List(CometSerializedFunc) methods, uint32_t numFields) {
    CometSerializedStruct* newStruct = malloc(sizeof(CometSerializedStruct));
    newStruct->numFields = numFields;
    newStruct->numMethods = methods.count;

    newStruct->vtable = malloc(sizeof(uint32_t) * newStruct->numMethods);
    for (size_t i = 0; i < newStruct->numMethods; i++) {
        newStruct->vtable[i] = *get(methods, i);
    }
    
    return newStruct;
}

CometOperand cometValue(CometValueTypeKind valueType, ...) {
    va_list args;
    va_start(args, valueType);

    CometOperand newVal;

    switch (valueType) {
        case COMET_SMALL: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_SMALL,
                .imm.smallVal = (int8_t)va_arg(args, int8_t)
            };
            break;
        case COMET_INT: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_INT,
                .imm.intVal = (int32_t)va_arg(args, int32_t)
            };
            break;
        case COMET_BIG: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_BIG,
                .imm.bigVal = (int64_t)va_arg(args, int64_t)
            };
            break;
        case COMET_FLOAT: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_FLOAT,
                .imm.floatVal = (float)va_arg(args, float)
            };
            break;
        case COMET_DOUBLE: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_DOUBLE,
                .imm.doubleVal = (double)va_arg(args, double)
            };
            break;
        case COMET_BOOL:
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_BOOL,
                .imm.boolVal = (bool)va_arg(args, bool)
            };
            break;
        case COMET_STRUCT:
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_STRUCT,
                .imm.objectVal = (CometObject*)va_arg(args, CometObject*)
            };
            break;
        case COMET_FUNCTION:
            newVal = (CometOperand){
                .type = CO_SYMBOL,
                .imm.typeKind = COMET_FUNCTION,
                .symbolIdx = va_arg(args, uint32_t)
            };
            break;
        case COMET_VOID: 
            newVal = (CometOperand){
                .type = CO_NONE
            };
            break;
        case COMET_MODULE:
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_MODULE,
                .imm.moduleVal = va_arg(args, CometEnvironment*)
            };
            break;
    }

    va_end(args);
    return newVal;
}