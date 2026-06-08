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
#include "util.h"

typedef void* voidPtr;

Result(voidPtr, charptr);
Result(CometType, charptr);
Result(CometFunctionTypeInfo, charptr);

ResultType(CometOperand, charptr) compile(CometCompiler* c, CometASTNode* node);
ResultType(cometCompilerPtr, charptr) newCompiler();
ResultType(voidPtr, charptr) outputToFile(CometCompiler* c, const char* filePath);
CometOperand createOperand(CometOperandKind type);



#endif