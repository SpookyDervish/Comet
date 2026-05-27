#ifndef SERIALIZED_H
#define SERIALIZED_H

#include <stdint.h>

typedef enum {
    INST_PUSH_CONST,
    INST_STORE,
    INST_LOAD,
    INST_ADD,
    INST_SUB,
    INST_MUL,
    INST_LOAD_ARG,
    INST_RET,
    INST_CALL,
    INST_EQ,
    INST_JMP,
    INST_JMP_IF_FALSE,
    INST_NOT
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
    char magic[5];
    uint8_t version;
    uint32_t numConsts;
    uint32_t numInstructions;
    uint32_t numFunctions;
} CometFile;

#endif