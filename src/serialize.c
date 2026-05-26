#include "serialize.h"
#include "inst.h"
#include "operand.h"
#include <stddef.h>
#include <stdlib.h>

uint32_t serializeOperand(CometCompiler* c, CometOperand operand) {
    switch (operand.type) {
        case CO_IMMEDIATE: {
            CometOperand constantIdx = findConst(c, operand);

            return constantIdx.imm.intVal;
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

CometSerializedInst* serializeInst(CometCompiler* c, CometInst inst) {
    CometSerializedInst* serialized = calloc(sizeof(CometSerializedInst), 1);
    serialized->opcode = inst.opcode;
    serialized->a = serializeOperand(c, inst.a);
    serialized->b = serializeOperand(c, inst.b);
    serialized->c = serializeOperand(c, inst.c);

    return serialized;
}