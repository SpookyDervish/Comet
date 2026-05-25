#include "inst.h"
#include <stdio.h>
#include <stdlib.h>

CometOperand createOperand(CometOperandKind type) {
    return (CometOperand){
        .type = type
    };
}

char* cometImmediateToCStr(CometImmediate immediate) {
    switch (immediate.typeKind) {
        case COMET_SMALL: {
            char* buffer = malloc(4);
            sprintf(buffer, "%d", immediate.smallVal);
            return buffer;
        }
        case COMET_INT: {
            char* buffer = malloc(32);
            sprintf(buffer, "%d", immediate.intVal);
            return buffer;
        }
        case COMET_BIG: {
            char* buffer = malloc(64);
            sprintf(buffer, "%ld", immediate.bigVal);
            return buffer;
        }

        case COMET_FLOAT: {
            char* buffer = malloc(128);
            sprintf(buffer, "%f", immediate.floatVal);
            return buffer;
        }
        case COMET_DOUBLE: {
            char* buffer = malloc(128);
            sprintf(buffer, "%f", immediate.doubleVal);
            return buffer;
        }

        case COMET_BOOL: {
            if (immediate.boolVal) {
                return "true";
            } else {
                return "false";
            }
        }

        case COMET_VOID: {
            return "void";
        }
    }
}

char* cometOperandToCStr(CometOperand operand) {
    switch (operand.type) {
        case CO_IMMEDIATE:
            return cometImmediateToCStr(operand.imm);

        case CO_STACK: {
            char* buffer = malloc(64);
            sprintf(buffer, "%%%d", operand.reg);
            return buffer;
        }
    }
}

char* cometInstOpcodeToCStr(CometInstType instType) {
    switch (instType) {
        case INST_MOV: return "MOV";
        case INST_ADD: return "ADD";
        default: return "FIXME";
    }
}

char* cometInstructionToCStr(CometInst inst) {
    char* buffer = malloc(256);

    sprintf(
        buffer,
        "%s    %s, %s, %s",
        cometInstOpcodeToCStr(inst.opcode),
        cometOperandToCStr(inst.dest),
        cometOperandToCStr(inst.a),
        cometOperandToCStr(inst.b)
    );

    return buffer;
}

void buildInst(
    CometCompiler* c,
    CometInstType opcode,
    CometOperand dest,
    CometOperand a,
    CometOperand b
) {
    c->outputProgram[c->programIdx] = (CometInst){
        .opcode = opcode,
        .dest = dest,
        .a = a,
        .b = b,
    };
    c->programIdx++;
}

CometOperand buildAdd(
    CometCompiler* c,
    CometOperand a,
    CometOperand b
) {
    CometOperand reg = createOperand(CO_STACK);
    reg.reg = c->stackIdx++;

    buildInst(c, INST_ADD, reg, a, b);

    return reg;
}

ResultType(cometCompilerPtr, charptr) newCompiler() {
    CometCompiler* newCompiler = calloc(sizeof(CometCompiler), 1);

    newCompiler->programIdx = 0;
    newCompiler->stackIdx = 0;

    return Success(cometCompilerPtr, charptr, newCompiler);
}