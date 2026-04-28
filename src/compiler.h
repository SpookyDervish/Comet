#ifndef COMPILER_H
#define COMPILER_H

#include "../include/error.h"
#include "ast.h"
#include "environment.h"
#include "parser.h"

#include <stddef.h>
#include <tram.h>

UseList(Tram_Register);

typedef struct {
    Tram_Parameter val;
    char* type;
} ValStructPair;

typedef struct {
    Tram_Program* program;
    Tram_Register* usedRegisters;
    Tram_Register* usedFloatRegisters;
    CometEnvironment* env;
} CometCompiler;

Result(CometCompiler, charptr);
Result(Tram_Register, charptr);
Result(ValStructPair, charptr);

ResultType(CometCompiler, charptr) createCompiler(CometParser* parser);
ResultType(Nothing, charptr) compileAST(CometCompiler* compiler, CometASTNode* root, char* outputName);
ResultType(Nothing, charptr) compile(CometCompiler* compiler, CometASTNode* node);

#endif