#include "../include/cometlib.h"
#include "../include/environment.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void cometDefineFunc(
    CometEnvironment* env,
    char* name,
    CometType returnType,
    uint32_t numArgs,
    bool isVarArgs,
    ...
) {
    CometFunction* func = malloc(sizeof(CometFunction));
    memcpy(func->name, name, 32);
    func->argCount = numArgs;
    func->isMethod = false;
    func->returnType = returnType;
    func->startIdx = 0;
    func->isExternal = true;
    func->isVarArgs = isVarArgs;
    
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

    va_list args;
    va_start(args, isVarArgs);

    CometType* argTypes = numArgs > 0 ? calloc(numArgs, sizeof(CometType)) : NULL;
    for (size_t i = 0; i < numArgs; i++) {
        argTypes[i] = va_arg(args, CometType);
    }

    func->argTypes = argTypes;

    va_end(args);

    defineVar(env, name, RECORD_LOCAL, funcVal, type, false);
}

int64_t serializeValue(CometOperand value) {
    if (value.type == CO_SYMBOL) {
        return value.symbolIdx;
    }

    switch (value.imm.typeKind) {
        case COMET_SMALL   : return value.imm.smallVal;
        case COMET_INT     : return value.imm.intVal;
        case COMET_BIG     : return value.imm.bigVal;
        case COMET_BOOL    : return value.imm.boolVal;
        case COMET_STRUCT  : return (int64_t)value.imm.objectVal;
        case COMET_FLOAT   : {
            int64_t serialized;
            memcpy(&serialized, &value.imm.floatVal, sizeof(float));
            return serialized;
        }
        case COMET_DOUBLE  : {
            int64_t serialized;
            memcpy(&serialized, &value.imm.doubleVal, sizeof(double));
            return serialized;
        }
        default: return 0;
    }
}

CometOperand deserializeValue(int64_t value, CometType type) {
    switch (type.typeKind) {
        case COMET_SMALL   : return (CometOperand){ .type = CO_IMMEDIATE, .imm.typeKind = COMET_SMALL,  .imm.smallVal = value };
        case COMET_INT     : return (CometOperand){ .type = CO_IMMEDIATE, .imm.typeKind = COMET_INT,    .imm.intVal = value };
        case COMET_BIG     : return (CometOperand){ .type = CO_IMMEDIATE, .imm.typeKind = COMET_BIG,    .imm.bigVal = value };
        case COMET_FLOAT   : return (CometOperand){ .type = CO_IMMEDIATE, .imm.typeKind = COMET_FLOAT,  .imm.floatVal = value };
        case COMET_DOUBLE  : return (CometOperand){ .type = CO_IMMEDIATE, .imm.typeKind = COMET_DOUBLE, .imm.doubleVal = value };
        case COMET_BOOL    : return (CometOperand){ .type = CO_IMMEDIATE, .imm.typeKind = COMET_BOOL,   .imm.boolVal = value };
        case COMET_FUNCTION: return (CometOperand){ .type = CO_SYMBOL, .symbolIdx = value };
        case COMET_STRUCT  : return (CometOperand){ .type = CO_IMMEDIATE, .imm.typeKind = COMET_STRUCT, .imm.objectVal = (CometObject*)value };
        case COMET_ARRAY   : {
            CometOperand deserializedValue = (CometOperand){ .type = CO_IMMEDIATE, .imm.typeKind = COMET_ARRAY};
            CometSerializedArray* array = (CometSerializedArray*)value;

            uint64_t capacity = array->capacity;

            CometOperand* deserializedData = malloc(sizeof(CometOperand) * capacity);

            deserializedValue.imm.arrayVal = (CometArray){
                .capacity = capacity,
                .data = deserializedData
            };

            int64_t* serializedData = array->data;
            for (size_t i = 0; i < capacity; i++) {
                deserializedData[i] = deserializeValue(serializedData[i], array->elemType);
            }

            return deserializedValue;
        };
        default: return  (CometOperand){ .type = CO_NONE };
    }
}

API_EXPORT void* cometArrayToCArray(CometOperand arrayValue, CometType elemType) {
    uint64_t capacity = arrayValue.imm.arrayVal.capacity;
    CometOperand* data = arrayValue.imm.arrayVal.data;

    void* cArray;
    switch (elemType.typeKind) {
        case COMET_SMALL: {
            cArray = calloc(capacity, sizeof(int8_t));

            for (size_t i = 0; i < capacity; i++) {
                ((int8_t*)cArray)[i] = data[i].imm.smallVal;
            }
            break;
        }
        case COMET_INT: {
            cArray = calloc(capacity, sizeof(int32_t));

            for (size_t i = 0; i < capacity; i++) {
                ((int32_t*)cArray)[i] = data[i].imm.intVal;
            }
            break;
        }
        case COMET_BIG: {
            cArray = calloc(capacity, sizeof(int64_t));

            for (size_t i = 0; i < capacity; i++) {
                ((int64_t*)cArray)[i] = data[i].imm.bigVal;
            }
            break;
        }
        case COMET_FLOAT: {
            cArray = calloc(capacity, sizeof(float));

            for (size_t i = 0; i < capacity; i++) {
                ((float*)cArray)[i] = data[i].imm.floatVal;
            }
            break;
        }
        case COMET_DOUBLE: {
            cArray = calloc(capacity, sizeof(double));

            for (size_t i = 0; i < capacity; i++) {
                ((double*)cArray)[i] = data[i].imm.doubleVal;
            }
            break;
        }
        case COMET_BOOL: {
            cArray = calloc(capacity, sizeof(bool));

            for (size_t i = 0; i < capacity; i++) {
                ((bool*)cArray)[i] = data[i].imm.boolVal;
            }
            break;
        }
        default: return NULL;
        
    }

    return cArray;
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
                .imm.smallVal = (int8_t)va_arg(args, int)
            };
            break;
        case COMET_INT: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_INT,
                .imm.intVal = (int32_t)va_arg(args, int)
            };
            break;
        case COMET_BIG: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_BIG,
                .imm.bigVal = (int64_t)va_arg(args, int)
            };
            break;
        case COMET_FLOAT: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_FLOAT,
                .imm.floatVal = (float)va_arg(args, double)
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
                .imm.boolVal = (bool)va_arg(args, int)
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
                .symbolIdx = (uint32_t)va_arg(args, int)
            };
            break;
        case COMET_VOID: 
            newVal = (CometOperand){
                .type = CO_NONE
            };
            break;
        case COMET_TYPE: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_TYPE,
                .imm.typeVal = va_arg(args, CometType)
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