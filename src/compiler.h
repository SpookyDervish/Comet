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
#include "typemap.h"

typedef void* voidPtr;


typedef List(astNodePtr) astNodeList;
typedef CometType* cometTypePtr;

Result(voidPtr, ErrorMessage);
Result(CometType, ErrorMessage);
Result(astNodeList, ErrorMessage);
Result(CometFunctionTypeInfo, ErrorMessage);
Result(cometCompilerPtr, ErrorMessage);
Result(cometTypePtr, ErrorMessage);


ResultType(CometOperand, ErrorMessage) compile(CometCompiler* c, CometASTNode* node);
ResultType(cometCompilerPtr, ErrorMessage) createCompiler(char* inputFilePath, char* sourceCode, bool debugSymbols);
ResultType(voidPtr, ErrorMessage) outputToFile(CometCompiler* c, const char* filePath, bool debugSymbols);
CometOperand createOperand(CometOperandKind type);

#endif