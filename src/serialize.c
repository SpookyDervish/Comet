#include "../include/serialized.h"
#include "inst.h"
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

CometSerializedStruct* serializeStruct(CometStruct* structType) {
    CometSerializedStruct* serialized = calloc(1, sizeof(CometSerializedStruct));
    
    *serialized = (CometSerializedStruct){
        .numFields = structType->fieldCount,
        .vtable = calloc(structType->numMethods, sizeof(CometSerializedFunc)),
        .numMethods = structType->numMethods
    };

    for (size_t i = 0; i < structType->numMethods; i++) {
        CometMethod* method = structType->vtable[i];

        CometSerializedFunc func = {};
        memcpy(func.name, method->name, sizeof(func.name));
        func.numArgs = method->argCount;
        func.startIdx = method->startIdx;
        func.symbolIdx = method->symbolIdx;
        func.isExternal = false;
        func.externalPtr = NULL; // TO FUTURE ME: DONT REMOVE THIS YOU MORON. IT BREAKS EVERYTHING AND I HAVE NO CLUE WHY!!!!!! 

        serialized->vtable[i] = func;
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