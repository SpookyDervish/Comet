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