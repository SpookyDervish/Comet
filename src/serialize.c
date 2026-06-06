#include "serialize.h"
#include "inst.h"
#include "../include/comet_operand.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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

CometSerializedStruct* serializeStruct(CometStruct* structType) {
    CometSerializedStruct* serialized = calloc(1, sizeof(CometSerializedStruct));
    
    *serialized = (CometSerializedStruct){
        .numFields = structType->fieldCount,
        .vtable = calloc(structType->numMethods, sizeof(uint32_t)),
        .numMethods = structType->numMethods
    };

    for (size_t i = 0; i < structType->numMethods; i++) {
        serialized->vtable[i] = structType->vtable[i]->symbolIdx;
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