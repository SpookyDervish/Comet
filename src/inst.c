#include "inst.h"
#include "environment.h"
#include "operand.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
            sprintf(buffer, "%lld", immediate.bigVal);
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
            char* buffer = malloc(32);
            sprintf(buffer, "%%%d", operand.stackIdx);
            return buffer;
        }

        case CO_SYMBOL: {
            char* buffer = malloc(64);
            sprintf(buffer, "func %s", operand.symbolName); 
            return buffer;
        }
    }
}

char* cometInstOpcodeToCStr(CometInstType instType) {
    switch (instType) {
        case INST_PUSH_CONST: return "PUSH_CONST  ";
        case INST_STORE     : return "STORE       ";
        case INST_LOAD      : return "LOAD        ";
        case INST_ADD       : return "ADD         ";
        case INST_SUB       : return "SUB         ";
        case INST_MUL       : return "MUL         ";
        case INST_LOAD_ARG  : return "LOAD_ARG    ";
        case INST_RET       : return "RET         ";
        case INST_CALL      : return "CALL        ";
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
                inst.a.imm.intVal,
                cometOperandToCStr(c->consts[inst.a.imm.intVal])
            );
            break;
        default: extra = "";
    }

    sprintf(
        buffer,
        "%s%s, %s, %s    %s",
        cometInstOpcodeToCStr(inst.opcode),
        cometOperandToCStr(inst.a),
        cometOperandToCStr(inst.b),
        cometOperandToCStr(inst.c),
        extra
    );

    return buffer;
}

void buildInst(
    CometCompiler* compiler,
    CometInstType opcode,
    CometOperand a,
    CometOperand b,
    CometOperand c
) {
    compiler->outputProgram[compiler->programIdx] = (CometInst){
        .opcode = opcode,
        .a = a,
        .b = b,
        .c = c
    };
    compiler->programIdx++;
}

ResultType(cometCompilerPtr, charptr) newCompiler() {
    CometCompiler* newCompiler = calloc(sizeof(CometCompiler), 1);

    newCompiler->programIdx = 0;
    newCompiler->stackIdx = 0;
    newCompiler->env = newEnvironment("root", NULL);

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

            case CO_SYMBOL: {
                if (strcmp(constValue.symbolName, value.symbolName) == 0) {
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
void buildStore(CometCompiler* c, uint32_t idx) {
    popVal(c);

    
    CometOperand value = createOperand(CO_IMMEDIATE);
    value.imm = (CometImmediate){
        .typeKind = COMET_INT,
        .intVal = idx
    };

    buildInst(c, INST_STORE, value, NO_OPERAND, NO_OPERAND);
}
CometOperand buildLoad(CometCompiler* c, uint32_t idx) {
    CometOperand value = createOperand(CO_IMMEDIATE);
    value.imm = (CometImmediate){
        .typeKind = COMET_INT,
        .intVal = idx
    };

    buildInst(c, INST_LOAD, value, NO_OPERAND, NO_OPERAND);

    return value;
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
CometOperand buildFunction(CometCompiler* c, char* name, uint32_t argCount) {
    CometFunction* newFunction = malloc(sizeof(CometFunction));
    newFunction->argCount = argCount;

    strncpy(newFunction->name, name, 32);
    newFunction->startIdx = c->programIdx;

    c->functions[c->functionCount] = newFunction;
    c->functionCount++;

    CometOperand funcValue = createOperand(CO_SYMBOL);
    funcValue.symbolName = name;

    return funcValue;
}
void buildReturn(CometCompiler* c, CometOperand value) {
    buildInst(c, INST_RET, NO_OPERAND, NO_OPERAND, NO_OPERAND);
}
CometOperand buildLoadArg(CometCompiler* c, uint32_t idx) {
    CometOperand argValue = pushVal(c);

    CometOperand indexOperand = createOperand(CO_IMMEDIATE);
    indexOperand.imm.typeKind = COMET_INT;
    indexOperand.imm.intVal = idx;

    buildInst(c, INST_LOAD_ARG, indexOperand, NO_OPERAND, NO_OPERAND);

    return argValue;
}
CometOperand buildCall(CometCompiler* c, char* name, List(CometOperand) args) {
    CometOperand funcValue = createOperand(CO_SYMBOL);
    funcValue.symbolName = name;

    CometOperand returnValue = pushVal(c);

    for (size_t argIdx = 0; argIdx < args.count; argIdx++) {
        CometOperand argValue = *get(args, argIdx);

        pushVal(c);
        //buildInst(c, INST_PUSH_ARG, argValue, NO_OPERAND, NO_OPERAND);
    }

    buildInst(c, INST_CALL, funcValue, NO_OPERAND, NO_OPERAND);
    return returnValue;
}