#include "inst.h"
#include "environment.h"
#include "../include/operand.h"
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
            sprintf(buffer, "%hbd", immediate.smallVal);
            return buffer;
        }
        case COMET_INT: {
            char* buffer = malloc(32);
            sprintf(buffer, "%d", immediate.intVal);
            return buffer;
        }
        case COMET_BIG: {
            char* buffer = malloc(64);
            sprintf(buffer, "%zu", immediate.bigVal);
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

        default: return "FIXME";
    }
}

CometFunction* getSymbol(CometCompiler* c, CometOperand symbolValue) {
    for (size_t i = 0; i < c->functionCount; i++) {
        if (i == symbolValue.symbolIdx) {
            return c->functions[i];
        }
    }

    return NULL;
}

char* cometOperandToCStr(CometCompiler* c, CometOperand operand) {
    switch (operand.type) {
        case CO_IMMEDIATE:
            return cometImmediateToCStr(operand.imm);

        case CO_STACK: {
            char* buffer = malloc(32);
            sprintf(buffer, "%%%d", operand.stackIdx);
            return buffer;
        }

        case CO_SYMBOL: {
            CometFunction* func = getSymbol(c, operand);

            char* buffer = malloc(64);
            sprintf(buffer, "func %s, %d args", func->name, func->argCount); 
            return buffer;
        }

        case CO_LABEL: {
            char* buffer = malloc(32);
            if (operand.label->resolved)
                sprintf(buffer, "0x%x", operand.label->pos);
            else
                sprintf(buffer, "(unresolved)");

            return buffer;
        }

        default: break;
    }

    return "FIXME";
}

char* cometInstOpcodeToCStr(CometInstType instType) {
    switch (instType) {
        case INST_PUSH_CONST   : return "    PUSH_CONST      ";
        case INST_STORE        : return "    STORE           ";
        case INST_LOAD         : return "    LOAD            ";
        case INST_ADD          : return "    ADD             ";
        case INST_SUB          : return "    SUB             ";
        case INST_MUL          : return "    MUL             ";
        case INST_LOAD_ARG     : return "    LOAD_ARG        ";
        case INST_RET          : return "    RET             ";
        case INST_CALL         : return "    CALL            ";
        case INST_EQ           : return "    EQ              ";
        case INST_JMP          : return "    JMP             ";
        case INST_JMP_IF_FALSE : return "    JMP_IF_FALSE    ";
        case INST_NOT          : return "    NOT             ";
        default                : return "    FIXME           ";
    }
}

char* cometInstructionToCStr(CometCompiler* c, CometInst inst) {
    char* buffer = malloc(256);
    char* extra = malloc(128);

    buffer[0] = 0;
    extra[0] = 0;

    // print extra info if we can
    switch (inst.opcode) {
        case INST_PUSH_CONST:
            
            sprintf(
                extra,
                "; consts[%d] = %s",
                inst.a.imm.intVal,
                cometOperandToCStr(c, c->consts[inst.a.imm.intVal])
            );
            break;

        case INST_JMP:
        case INST_JMP_IF_FALSE:
            if (inst.a.label->pos >= c->programIdx) {
                sprintf(extra, "; (???)");
                break;
            }

            sprintf(
                extra,
                "; (%s)",
                cometInstructionToCStr(c, c->outputProgram[inst.a.label->pos])
            );
            break;

        default: break;
    }

    char* argsBuffer = malloc(128);
    argsBuffer[0] = 0;
    if (inst.a.type != CO_NONE)
        sprintf(argsBuffer, "%s", cometOperandToCStr(c, inst.a));    
    if (inst.b.type != CO_NONE)
        sprintf(argsBuffer + strlen(argsBuffer), ", %s", cometOperandToCStr(c, inst.b));    
    if (inst.c.type != CO_NONE)
        sprintf(argsBuffer + strlen(argsBuffer), ", %s", cometOperandToCStr(c, inst.c));    


    sprintf(
        buffer,
        "0x%04x%s%s    %s",
        inst.pos,
        cometInstOpcodeToCStr(inst.opcode),
        argsBuffer,

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
        .c = c,
        .pos = compiler->programIdx
    };
    compiler->programIdx++;
}

ResultType(cometCompilerPtr, charptr) newCompiler() {
    CometCompiler* newCompiler = calloc(1, sizeof(CometCompiler));

    newCompiler->outputProgram = calloc(2048, sizeof(CometInst));
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
        default: break;;
        
    }

    return false;
}

uint32_t getSymbolIndex(CometCompiler* c, const char* symbolName) {
    for (size_t i = 0; i < c->functionCount; i++) {
        CometFunction* func = c->functions[i];

        if (strcmp(func->name, symbolName) == 0) {
            return i;
        }
    }

    return -1;
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


                if (constValue.symbolIdx == value.symbolIdx) {
                    return constIdx;
                }
                break;
            }

            default: break;
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
CometOperand buildEq(CometCompiler* c) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    buildInst(c, INST_EQ, NO_OPERAND, NO_OPERAND, NO_OPERAND);

    return dest;
}
CometOperand buildFunction(CometCompiler* c, char* name, uint32_t argCount) {
    CometFunction* newFunction = malloc(sizeof(CometFunction));
    newFunction->argCount = argCount;

    strncpy(newFunction->name, name, 32);
    newFunction->startIdx = c->programIdx;

    c->functions[c->functionCount] = newFunction;

    CometOperand funcValue = createOperand(CO_SYMBOL);
    funcValue.symbolIdx = c->functionCount;

    c->functionCount++;

    return funcValue;
}
void buildReturn(CometCompiler* c) {
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
    funcValue.symbolIdx = getSymbolIndex(c, name);

    CometOperand returnValue = pushVal(c);

    for (size_t argIdx = 0; argIdx < args.count; argIdx++) {
        pushVal(c);
    }

    buildInst(c, INST_CALL, funcValue, NO_OPERAND, NO_OPERAND);
    return returnValue;
}
void buildJump(CometCompiler* c, CometLabel* label) {
    CometOperand labelOperand = createOperand(CO_LABEL);
    labelOperand.label = label;

    buildInst(c, INST_JMP, labelOperand, NO_OPERAND, NO_OPERAND);
}
void buildJumpIfFalse(CometCompiler* c, CometLabel* label) {
    CometOperand labelOperand = createOperand(CO_LABEL);
    labelOperand.label = label;

    popVal(c);

    buildInst(c, INST_JMP_IF_FALSE, labelOperand, NO_OPERAND, NO_OPERAND);
}
CometOperand buildNot(CometCompiler* c) {
    popVal(c);
    CometOperand dest = pushVal(c);

    

    buildInst(c, INST_NOT, NO_OPERAND, NO_OPERAND, NO_OPERAND);

    return dest;
}

// -- LABELS -- //
CometLabel* buildLabel(CometCompiler* c) {
    CometLabel* newLabel = malloc(sizeof(CometLabel));
    newLabel->resolved = false;

    c->labels[c->labelCount] = newLabel;
    c->labelCount++;

    return newLabel;
}
void resolveLabel(CometCompiler* c, CometLabel* label) {
    label->pos = c->programIdx;
    label->resolved = true;
}