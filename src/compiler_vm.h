#ifndef VM_H
#define VM_H

#include "ast.h"
#include "lexer.h"
#include "inst.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../lib/error.h"
#include "../include/comet_operand.h"

typedef void* voidPtr;

Result(voidPtr, charptr);

typedef struct {
    char magic[5];
    uint8_t version;
    uint32_t numConsts;
    uint32_t numInstructions;
    uint32_t numFunctions;
    uint32_t numStructs;
} CometFile;

Result(CometType, charptr);
Result(CometFunctionTypeInfo, charptr);

ResultType(CometOperand, charptr) compile(CometCompiler* c, CometASTNode* node);
ResultType(voidPtr, charptr) outputToFile(CometCompiler* c, const char* filePath);
CometOperand createOperand(CometOperandKind type);



#endif