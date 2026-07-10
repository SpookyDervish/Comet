#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"
#include "lexer.h"
#include "inst.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../include/error.h"
#include "../include/serialized.h"
#include "../include/comet_operand.h"
#include "../include/environment.h"
#include "../include/util.h"
#include "../include/debug.h"
#include "../include/cometlib.h"
#include "../include/typemap.h"

typedef void* voidPtr;

typedef struct {
    CometOperand value;
    bool fallsThrough;
} CompiledValue;

#define COMPILED_VALUE(val, ft) ((CompiledValue){ .value = (val), .fallsThrough = (ft) })
#define FALLS_THROUGH(val) (CompiledValue){ .value = val, .fallsThrough = true }
#define NO_VALUE ((CompiledValue){ .value = NO_OPERAND, .fallsThrough = true })

typedef List(astNodePtr) astNodeList;
typedef CometType* cometTypePtr;

Result(CompiledValue, ErrorMessage);
Result(voidPtr, ErrorMessage);
Result(CometType, ErrorMessage);
Result(astNodeList, ErrorMessage);
Result(CometFunctionTypeInfo, ErrorMessage);
Result(cometCompilerPtr, ErrorMessage);
Result(cometTypePtr, ErrorMessage);

ResultType(CometType, ErrorMessage) resolveType(CometCompiler* c, CometASTNode* node);
ResultType(CompiledValue, ErrorMessage) compile(CometCompiler* c, CometASTNode* node);
ResultType(cometCompilerPtr, ErrorMessage) createCompiler(char* inputFilePath, char* sourceCode, bool debugSymbols);
ResultType(voidPtr, ErrorMessage) outputToFile(CometCompiler* c, const char* filePath, bool debugSymbols);
CometOperand createOperand(CometOperandKind type);

#endif