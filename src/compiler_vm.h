#ifndef VM_H
#define VM_H

#include "ast.h"
#include "lexer.h"
#include "inst.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../include/error.h"

typedef void* voidPtr;

Result(voidPtr, charptr);

ResultType(CometOperand, charptr) compile(CometCompiler* c, CometASTNode* node);
CometOperand createOperand(CometOperandKind type);



#endif