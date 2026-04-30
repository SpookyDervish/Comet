#ifndef COMPILER_H
#define COMPILER_H

#include "../include/error.h"
#include "ast.h"
#include "environment.h"
#include "parser.h"
#include <stddef.h>

typedef struct {
    CometEnvironment* env;
} CometCompiler;

Result(CometCompiler, charptr);

ResultType(CometCompiler, charptr) createCompiler(CometParser* parser);
ResultType(Nothing, charptr) compileAST(CometCompiler* compiler, CometASTNode* root, char* outputName);
ResultType(Nothing, charptr) compile(CometCompiler* compiler, CometASTNode* node);

#endif