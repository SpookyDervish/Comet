#include "inst.h"
#include "../include/comet_operand.h"
#include "../include/type.h"
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CometOperand createOperand(CometOperandKind type) {
    return (CometOperand){
        .type = type
    };
}

CometFunction* getSymbol(CometCompiler* c, CometOperand symbolValue) {
    for (size_t i = 0; i < c->functionCount; i++) {
        if (i == symbolValue.symbolIdx) {
            return c->functions[i];
        }
    }

    return NULL;
}

CometType getValueType(CometCompiler* c, CometOperand value) {
    switch (value.type) {
        case CO_IMMEDIATE: {
            return (CometType){
                .typeKind = value.imm.typeKind
            };
        }
        case CO_SYMBOL: {
            // find symbol
            CometFunction* func = c->functions[value.symbolIdx];
            return (CometType){
                .typeKind = COMET_FUNCTION,
                .functionType = func
            };
        }
        default: {
            return (CometType){
                .typeKind = COMET_VOID
            };
        }
    }
}

void buildInst(
    CometCompiler* compiler,
    CometInstType opcode,
    CometOperand a,
    CometOperand b,
    CometOperand c
) {
    append(compiler->debugInstInfo, compiler->currentLine);

    CometInst newInst = {
        .opcode = opcode,
        .a = a,
        .b = b,
        .c = c
    };

    append(compiler->currentBlock->instructions, newInst);
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
        default: break;
        
    }

    return false;
}

int32_t getSymbolIndex(CometCompiler* c, const char* symbolName) {
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

bool typeIsInt(CometType type) {
    switch (type.typeKind) {
        case COMET_SMALL:
        case COMET_INT:
        case COMET_BIG:
        case COMET_BOOL:
            return true;
        default:
            return false;
    }
}

inline bool typeIsFloat(CometType type) {
    if (type.typeKind == COMET_FLOAT || type.typeKind == COMET_DOUBLE)
        return true;
    return false;
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
CometOperand buildAdd(CometCompiler* c, CometType resultType) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    if (typeIsInt(resultType)) {
        buildInst(c, INST_ADDI, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    } else {
        buildInst(c, INST_ADDF, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    }

    return dest;
}
CometOperand buildSub(CometCompiler* c, CometType resultType) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    if (typeIsInt(resultType)) {
        buildInst(c, INST_SUBI, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    } else {
        buildInst(c, INST_SUBF, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    }

    return dest;
}
CometOperand buildMul(CometCompiler* c, CometType resultType) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    if (typeIsInt(resultType)) {
        buildInst(c, INST_MULI, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    } else {
        buildInst(c, INST_MULF, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    }

    return dest;
}
CometOperand buildDiv(CometCompiler* c, CometType resultType) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    if (typeIsInt(resultType)) {
        buildInst(c, INST_DIVI, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    } else {
        buildInst(c, INST_DIVF, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    }

    return dest;
}
CometOperand buildEq(CometCompiler* c, CometType resultType) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    if (typeIsInt(resultType)) {
        buildInst(c, INST_EQI, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    } else {
        buildInst(c, INST_EQF, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    }

    return dest;
}
CometOperand buildNeq(CometCompiler* c, CometType resultType) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    if (typeIsInt(resultType)) {
        buildInst(c, INST_NEQI, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    } else {
        buildInst(c, INST_NEQF, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    }

    return dest;
}
CometOperand buildLt(CometCompiler* c, CometType resultType) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    if (typeIsInt(resultType)) {
        buildInst(c, INST_LTI, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    } else {
        buildInst(c, INST_LTF, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    }

    return dest;
}
CometOperand buildGt(CometCompiler* c, CometType resultType) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    if (typeIsInt(resultType)) {
        buildInst(c, INST_GTI, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    } else {
        buildInst(c, INST_GTF, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    }

    return dest;
}
CometOperand buildLte(CometCompiler* c, CometType resultType) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    if (typeIsInt(resultType)) {
        buildInst(c, INST_LTEI, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    } else {
        buildInst(c, INST_LTEF, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    }

    return dest;
}
CometOperand buildGte(CometCompiler* c, CometType resultType) {
    popVal(c);
    popVal(c);

    CometOperand dest = pushVal(c);

    if (typeIsInt(resultType)) {
        buildInst(c, INST_GTEI, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    } else {
        buildInst(c, INST_GTEF, NO_OPERAND, NO_OPERAND, NO_OPERAND);
    }

    return dest;
}
CometOperand buildFunction(CometCompiler* c, char* name, uint32_t argCount, CometType returnType, CometType* argTypes, bool isVarArgs, bool isMethod, bool isExternal, int8_t libIdx) {
    CometFunction* newFunction = malloc(sizeof(CometFunction));
    newFunction->argCount = argCount;
    newFunction->returnType = returnType;
    newFunction->isMethod = isMethod;
    newFunction->isExternal = isExternal;
    newFunction->libIdx = libIdx;
    newFunction->argTypes = argTypes;
    newFunction->isVarArgs = isVarArgs;

    strncpy(newFunction->name, name, 31);
    //newFunction->startIdx = c->programIdx;

    newFunction->blockIdx = c->blocks.count;
    startBlock(c);

    c->functions[c->functionCount] = newFunction;

    CometOperand funcValue = createOperand(CO_SYMBOL);
    funcValue.symbolIdx = c->functionCount;

    c->functionCount++;

    c->currentFunction = newFunction;

    return funcValue;
}
void buildReturn(CometCompiler* c) {
    buildInst(c, INST_RET, NO_OPERAND, NO_OPERAND, NO_OPERAND);
}
CometOperand buildLoadArg(CometCompiler* c, uint32_t idx) {
    CometOperand argValue = pushVal(c);

    CometOperand indexOperand = createOperand(CO_IMMEDIATE);
    indexOperand.imm.typeKind = COMET_SMALL;
    indexOperand.imm.smallVal = idx;

    buildInst(c, INST_LOAD_ARG, indexOperand, NO_OPERAND, NO_OPERAND);

    return argValue;
}
CometOperand buildCall(CometCompiler* c, char* name, List(CometOperand) args) {
    CometOperand funcValue = createOperand(CO_SYMBOL);
    funcValue.symbolIdx = getSymbolIndex(c, name);

    assert(funcValue.symbolIdx != -1);

    CometOperand returnValue = pushVal(c);

    for (size_t argIdx = 0; argIdx < args.count; argIdx++) {
        pushVal(c);
    }

    CometOperand numArgs = createOperand(CO_IMMEDIATE);
    numArgs.imm.typeKind = COMET_SMALL;
    numArgs.imm.smallVal = args.count;

    buildInst(c, INST_CALL, funcValue, numArgs, NO_OPERAND);
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
void buildJumpIfTrue(CometCompiler* c, CometLabel* label) {
    CometOperand labelOperand = createOperand(CO_LABEL);
    labelOperand.label = label;

    popVal(c);

    buildInst(c, INST_JMP_IF_TRUE, labelOperand, NO_OPERAND, NO_OPERAND);
}
CometOperand buildNot(CometCompiler* c) {
    popVal(c);
    CometOperand dest = pushVal(c);

    

    buildInst(c, INST_NOT, NO_OPERAND, NO_OPERAND, NO_OPERAND);

    return dest;
}
CometOperand buildI2F(CometCompiler* c) {
    popVal(c);
    CometOperand dest = pushVal(c);

    buildInst(c, INST_I2F, NO_OPERAND, NO_OPERAND, NO_OPERAND);

    return dest;
}
CometOperand buildF2I(CometCompiler* c) {
    pushVal(c);
    CometOperand dest = pushVal(c);

    buildInst(c, INST_F2I, NO_OPERAND, NO_OPERAND, NO_OPERAND);

    return dest;
}
void buildDup(CometCompiler* c) {
    buildInst(c, INST_DUP, NO_OPERAND, NO_OPERAND, NO_OPERAND);
}
CometOperand buildNew(CometCompiler* c, uint32_t idx) {
    CometOperand dest = pushVal(c);

    CometOperand indexOperand = createOperand(CO_IMMEDIATE);
    indexOperand.imm.typeKind = COMET_INT;
    indexOperand.imm.intVal = idx;

    buildInst(c, INST_NEW, indexOperand, NO_OPERAND, NO_OPERAND);

    return dest;
}

CometOperand buildBuildList(CometCompiler* c) {
    popVal(c); // pop size

    CometOperand dest = pushVal(c);

    buildInst(c, INST_BUILD_LIST, NO_OPERAND, NO_OPERAND, NO_OPERAND);

    return dest;
}

CometOperand buildListAt(CometCompiler* c) {
    CometOperand dest = pushVal(c);

    buildInst(c, INST_LIST_AT, NO_OPERAND, NO_OPERAND, NO_OPERAND);

    return dest;
}

void buildListSet(CometCompiler* c) {
    buildInst(c, INST_LIST_SET, NO_OPERAND, NO_OPERAND, NO_OPERAND);
}

CometOperand buildGetField(CometCompiler* c, uint32_t idx) {
    CometOperand dest = pushVal(c);

    CometOperand indexOperand = createOperand(CO_IMMEDIATE);
    indexOperand.imm.typeKind = COMET_INT;
    indexOperand.imm.intVal = idx;

    buildInst(c, INST_GET_FIELD, indexOperand, NO_OPERAND, NO_OPERAND);

    return dest;
}
void buildSetField(CometCompiler* c, uint32_t idx) {
    CometOperand indexOperand = createOperand(CO_IMMEDIATE);
    indexOperand.imm.typeKind = COMET_INT;
    indexOperand.imm.intVal = idx;

    buildInst(c, INST_SET_FIELD, indexOperand, NO_OPERAND, NO_OPERAND);
}
CometOperand buildCallMethod(CometCompiler* c, uint32_t vtableIdx, List(CometOperand) args) {
    CometOperand funcValue = createOperand(CO_IMMEDIATE);
    funcValue.imm.typeKind = COMET_SMALL;
    funcValue.imm.smallVal = vtableIdx;

    CometOperand returnValue = pushVal(c);

    for (size_t argIdx = 0; argIdx < args.count; argIdx++) {
        pushVal(c);
    }

    CometOperand numArgs = createOperand(CO_IMMEDIATE);
    numArgs.imm.typeKind = COMET_SMALL;
    numArgs.imm.smallVal = args.count;

    buildInst(c, INST_CALL_METHOD, funcValue, numArgs, NO_OPERAND);
    return returnValue;
}
void buildBreakpoint(CometCompiler* c) {
    buildInst(c, INST_BREAKPOINT, NO_OPERAND, NO_OPERAND, NO_OPERAND);
}
void buildTry(CometCompiler* c) {
    buildInst(c, INST_TRY, NO_OPERAND, NO_OPERAND, NO_OPERAND);
}
void buildEndTry(CometCompiler* c) {
    buildInst(c, INST_END_TRY, NO_OPERAND, NO_OPERAND, NO_OPERAND);
}
void buildThrow(CometCompiler* c) {
    buildInst(c, INST_THROW, NO_OPERAND, NO_OPERAND, NO_OPERAND);
}
CometOperand buildListLength(CometCompiler* c) {
    CometOperand dest = pushVal(c);

    buildInst(c, INST_LIST_LENGTH, NO_OPERAND, NO_OPERAND, NO_OPERAND);

    return dest;
}
CometType buildCast(CometCompiler* c, CometType before, CometType after) {
    if (typeIsInt(before) && typeIsFloat(after)) {
        buildI2F(c);
        before = after;
    }

    if (typeIsFloat(before) && typeIsInt(after)) {
        buildF2I(c);
        before = after;
    }

    return before;
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
    label->blockPos = c->currentBlock->instructions.count;
    //label->resolved = true;
}

// -- BLOCKS -- //
Block* startBlock(CometCompiler* c) {
    Block newBlock = {
        .instructions = newList(CometInst),
        .parent = c->currentBlock
    };
    append(c->blocks, newBlock);

    c->currentBlock = &c->blocks.pointer[c->blocks.count-1];
    return c->currentBlock;
}

void endBlock(CometCompiler* c) {
    c->currentBlock = c->currentBlock->parent;
}