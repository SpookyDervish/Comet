#ifndef SERIALIZED_H
#define SERIALIZED_H

#include <stdint.h>

typedef enum {
    INST_PUSH_CONST,
    INST_STORE,
    INST_LOAD,
    INST_ADDI,
    INST_ADDF,
    INST_SUBI,
    INST_SUBF,
    INST_MULI,
    INST_MULF,
    INST_DIVI,
    INST_DIVF,
    INST_EQI,
    INST_EQF,
    INST_NEQI,
    INST_NEQF,
    INST_GTI,
    INST_GTF,
    INST_LTI,
    INST_LTF,
    INST_GTEI,
    INST_GTEF,
    INST_LTEI,
    INST_LTEF,
    INST_LOAD_ARG,
    INST_RET,
    INST_CALL,
    INST_JMP,
    INST_JMP_IF_FALSE,
    INST_NOT,
    INST_I2F,
    INST_DUP,
    INST_NEW,
    INST_GET_FIELD,
    INST_SET_FIELD,
    INST_CALL_METHOD,
    INST_BREAKPOINT,
    INST_MAX
} CometInstType;

typedef struct {
    char name[32];
    uint32_t startIdx;
    uint32_t numArgs;
} CometSerializedFunc;

typedef struct {
    CometInstType opcode;
    int32_t a;
    int32_t b;
    int32_t c;
} CometSerializedInst;

typedef struct {
    uint32_t numFields;
    uint32_t numMethods;
    uint32_t* vtable;
} CometSerializedStruct;

typedef struct {
    uint32_t numFields;
    uint32_t numMethods;
} CometSerializedStructHeader;

typedef struct {
    char magic[5];
    uint8_t version;
    uint32_t numConsts;
    uint32_t numInstructions;
    uint32_t numFunctions;
    uint32_t numStructs;
} CometFile;

#endif