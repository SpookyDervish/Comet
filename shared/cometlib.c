#include "../include/cometlib.h"
#include "../include/environment.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
        case COMET_ARRAY   : {
            uint64_t capacity = value.imm.arrayVal.capacity;

            CometSerializedArray* arr = malloc(sizeof(CometSerializedArray));
            if (!arr)
                return 0;

            arr->data = malloc(sizeof(int64_t) * capacity);
            if (!arr->data)
                return 0;

            for (size_t i = 0; i < capacity; i++) {
                arr->data[i] = serializeValue(value.imm.arrayVal.data[i]);
            }
            arr->capacity = capacity;
            arr->elemType = cometTypeBig;

            return (int64_t)arr;
        }
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
        case COMET_FLOAT   : {
            CometOperand val = { .type = CO_IMMEDIATE, .imm.typeKind = COMET_FLOAT };

            float out;
            memcpy(&out, &value, sizeof(float));
            val.imm.floatVal = out;

            return val;
        }
        case COMET_DOUBLE  : {
            CometOperand val = { .type = CO_IMMEDIATE, .imm.typeKind = COMET_DOUBLE };

            double out;
            memcpy(&out, &value, sizeof(double));
            val.imm.doubleVal = out;

            return val;
        }
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

size_t GetCometTypeSize(CometType type) {
    switch (type.typeKind) {
        case COMET_SMALL: case COMET_BOOL:   return 1;
        case COMET_INT:   case COMET_FLOAT:  return 4;
        case COMET_BIG:   case COMET_DOUBLE: return 8;
        default: return 0;
    }
}

CometOperand CArrayToCometArray(void* arrayValue, size_t length, CometType elemType) {
    CometOperand out = {
        .type = CO_IMMEDIATE
    };

    CometOperand* serializedData = calloc(length, sizeof(CometOperand));
    if (!serializedData)
        return (CometOperand){.type = CO_NONE};

    size_t elemSize = GetCometTypeSize(elemType);

    uint8_t* bytePtr = (uint8_t*)arrayValue;

    for (size_t i = 0; i < length; i++) {
        void* elemAddr = bytePtr + (i * elemSize);
        int64_t extractedValue = 0;

        switch (elemSize) {
            case 1: extractedValue = *(int8_t*)elemAddr; break;
            case 2: extractedValue = *(int16_t*)elemAddr; break;
            case 4: extractedValue = *(int32_t*)elemAddr; break;
            case 8: extractedValue = *(int64_t*)elemAddr; break;
            default: break;
        }

        serializedData[i] = deserializeValue(extractedValue, elemType);
    }

    out.imm.typeKind = COMET_ARRAY;
    out.imm.arrayVal = (CometArray){
        .capacity = length,
        .data = serializedData
    };

    return out;
}

CometType createArrayType(CometType elem, uint8_t dimensions, bool isFixedSize[], uint64_t fixedSize[]) {
    CometType* elemPtr = malloc(sizeof(CometType));
    if (!elemPtr) return cometTypeVoid;

    *elemPtr = elem;

    CometArrayType* arrayType = malloc(sizeof(CometArrayType));
    if (!arrayType) return cometTypeVoid;

    arrayType->elem = elemPtr;
    arrayType->dims = dimensions;
    for (size_t i = 0; i < dimensions; i++) {
        if (isFixedSize[i]) {
            arrayType->isFixedSize[i] = true;
            arrayType->fixedSize[i] = fixedSize[i];
        } else {
            arrayType->isFixedSize[i] = false;

        }
    }

    memcpy(arrayType->isFixedSize, isFixedSize, sizeof(bool) * dimensions);
    memcpy(arrayType->fixedSize, fixedSize, sizeof(uint64_t) * dimensions);
    
    return (CometType){
        .typeKind = COMET_ARRAY,
        .arrayType = arrayType
    };
}