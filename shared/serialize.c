#include "../include/serialized.h"
#include "../include/comet_operand.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint32_t serializeOperand(CometOperand operand) {
    switch (operand.type) {
        case CO_IMMEDIATE: {
            
            return operand.imm.smallVal;
        }

        case CO_LABEL:
            return operand.label->pos;

        case CO_SYMBOL: {
            return operand.symbolIdx;
        }

        case CO_STACK:
            return operand.stackIdx;

        default: return 0;
    }
}

CometSerializedStruct* serializeStruct(CometFunction** compilerFuncs, CometStruct* structType) {
    CometSerializedStruct* serialized = calloc(1, sizeof(CometSerializedStruct));
    
    *serialized = (CometSerializedStruct){
        .numFields = structType->fieldCount,
        .vtable = calloc(structType->numMethods, sizeof(CometSerializedFunc)),
        .numMethods = structType->numMethods
    };

    size_t nameLen = strlen(structType->name) + 1;
    memcpy(serialized->name, structType->name, nameLen < 48 ? nameLen : 48);

    for (size_t i = 0; i < structType->numMethods; i++) {
        CometMethod* method = structType->vtable[i];
        serialized->vtable[i] = method->symbolIdx;
    }

    return serialized;
    
}

CometSerializedInst* serializeInst(CometInst inst) {
    CometSerializedInst* serialized = calloc(1, sizeof(CometSerializedInst));
    serialized->opcode = inst.opcode;
    serialized->a = serializeOperand(inst.a);
    serialized->b = serializeOperand(inst.b);
    serialized->c = serializeOperand(inst.c);

    return serialized;
}

size_t getCometTypeSize(CometType type) {
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

    size_t elemSize = getCometTypeSize(elemType);

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

        serializedData[i] = cometDeserializeValue(extractedValue, elemType);
    }

    out.imm.typeKind = COMET_ARRAY;
    out.imm.arrayVal = (CometArray){
        .capacity = length,
        .data = serializedData
    };

    return out;
}

int64_t cometSerializeValue(CometOperand value) {
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
                arr->data[i] = cometSerializeValue(value.imm.arrayVal.data[i]);
            }
            arr->capacity = capacity;
            arr->elemType = (CometType){ .typeKind = COMET_BIG };

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

CometOperand cometDeserializeValue(int64_t value, CometType type) {
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
                deserializedData[i] = cometDeserializeValue(serializedData[i], array->elemType);
            }

            return deserializedValue;
        };
        default: return  (CometOperand){ .type = CO_NONE };
    }
}

void* cometArrayToCArray(CometOperand arrayValue, CometType elemType) {
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