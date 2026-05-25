#include "inst.h"
#include <stdint.h>
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
            sprintf(buffer, "%%%d", operand.stackIdx);
            return buffer;
        }
    }
}

char* cometInstOpcodeToCStr(CometInstType instType) {
    switch (instType) {
        case INST_PUSH_CONST: return "PUSH_CONST  ";
        case INST_ADD       : return "ADD         ";
        case INST_SUB       : return "SUB         ";
        case INST_MUL       : return "MUL         ";
        default             : return "FIXME       ";
    }
}

char* cometInstructionToCStr(CometCompiler* c, CometInst inst) {
    char* buffer = malloc(256);
    char* extra = malloc(128);

    // print extra info if we can
    switch (inst.opcode) {
        case INST_PUSH_CONST:
            sprintf(
                extra,
                "; consts[%d] = %s",
                inst.dest.imm.intVal,
                cometOperandToCStr(c->consts[inst.dest.imm.intVal])
            );
            break;
        default: extra = "";
    }

    sprintf(
        buffer,
        "%s%s, %s, %s    %s",
        cometInstOpcodeToCStr(inst.opcode),
        cometOperandToCStr(inst.dest),
        cometOperandToCStr(inst.a),
        cometOperandToCStr(inst.b),
        extra
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

ResultType(cometCompilerPtr, charptr) newCompiler() {
    CometCompiler* newCompiler = calloc(sizeof(CometCompiler), 1);

    newCompiler->programIdx = 0;
    newCompiler->stackIdx = 0;

    return Success(cometCompilerPtr, charptr, newCompiler);
}

CometOperand pushVal(CometCompiler* c) {
    CometOperand dest = createOperand(CO_STACK);
    dest.stackIdx = c->stackIdx;
    c->stackIdx++;
    return dest;
}

inline void popVal(CometCompiler* c) {
    c->stackIdx--;
}

bool immediatesAreEqual(CometImmediate a, CometImmediate b) {
    if (a.typeKind != b.typeKind) return false;

    switch (a.typeKind) {
        case COMET_SMALL: return a.smallVal == b.smallVal;
        case COMET_INT: return a.intVal == b.intVal;
        case COMET_BIG: return a.bigVal == b.bigVal;
        case COMET_BOOL: return a.boolVal == b.boolVal;
        case COMET_FLOAT: return a.floatVal == b.floatVal;
        case COMET_DOUBLE: return a.doubleVal == b.doubleVal;
        case COMET_VOID: return true;
    }
}

CometOperand findConst(CometCompiler* c, CometOperand value) {
    for (uint32_t i = 0; i < c->constIdx; i++) {
        CometOperand constValue = c->consts[i];

        if (constValue.type != value.type) {
            continue;
        }

        CometOperand constIdx = createOperand(CO_IMMEDIATE);
        constIdx.imm = (CometImmediate){
            .typeKind = COMET_INT,
            .intVal = i
        };

        switch (constValue.type) {
            case CO_IMMEDIATE: {
                if (immediatesAreEqual(constValue.imm, value.imm)) {
                    return constIdx;
                }
                break;
            }

            case CO_STACK: {
                if (constValue.stackIdx == value.stackIdx) {
                    return constIdx;
                }
                break;
            }
        }
    }

    return NO_OPERAND;
}

// -- INSTRUCTIONS -- //
CometOperand storeConst(CometCompiler* c, CometOperand value) {
      

    // see if a constant with the same value already exists
    CometOperand existingConst = findConst(c, value);
    if (existingConst.imm.typeKind != COMET_VOID) {
        return existingConst;
    }

    // if it doesn't already exist create it
    CometOperand newConstIdx = createOperand(CO_IMMEDIATE);
    newConstIdx.imm = (CometImmediate){
        .typeKind = COMET_INT,
        .intVal = c->constIdx
    };

    c->consts[c->constIdx] = value;
    c->constIdx++;

    return newConstIdx;
}
void buildPushConst(CometCompiler* c, CometOperand idx) {
    pushVal(c);

    buildInst(c, INST_PUSH_CONST, idx, NO_OPERAND, NO_OPERAND);
}
CometOperand buildAdd(CometCompiler* c) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    buildInst(c, INST_ADD, NO_OPERAND, NO_OPERAND, NO_OPERAND);

    return dest;
}
CometOperand buildSub(CometCompiler* c) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    buildInst(c, INST_SUB, NO_OPERAND, NO_OPERAND, NO_OPERAND);

    return dest;
}
CometOperand buildMul(CometCompiler* c) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    buildInst(c, INST_MUL, NO_OPERAND, NO_OPERAND, NO_OPERAND);

    return dest;
}