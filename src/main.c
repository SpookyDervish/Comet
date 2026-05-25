#include "compiler_vm.h"
#include "inst.h"
#include "lexer.h"
#include "parser.h"
#include "args.h"
#include "util.h"
#include <stddef.h>
#include <stdio.h>
#include "../include/ansi.h"


const char* VERSION_NUMBER = "0.1.0";


int main(int argc, char** argv) {
    // parse command line args
    ResultType(CometArgs, charptr) args = parseArgs(argc, argv);
    if (args.error) {
        fprintf(stderr, "error: %s\n", args.as.error);
        return 1;
    }

    // print version if we want
    if (args.as.success.showVersion) {
        printf("Comet - Version %s\n", VERSION_NUMBER);
        printf(ESC_DIM "The programming language to fix em' all.\n");
        return 0;
    }

    // compile file
    char* source = getFileContents(args.as.success.filePath);

    ResultType(CometLexer, charptr) lexer = newLexer(source);
    ResultType(tokenList, charptr) tokens = lex(&lexer.as.success);

    if (tokens.error) {
        fprintf(stderr, "lexer error: %s\n", tokens.as.error);
        return 1;
    }
    
    ResultType(parserPtr, charptr) parser = newParser(tokens.as.success);
    if (parser.error) {
        fprintf(stderr, "error while creating parser: %s\n", parser.as.error);
        return 1;
    }

    ResultType(astNodePtr, charptr) ast = buildAST(parser.as.success);
    if (ast.error) {
        fprintf(stderr, "error while building ast: %s\n", ast.as.error);
        return 1;
    }

    ResultType(cometCompilerPtr, charptr) compiler = newCompiler();
    if (compiler.error) {
        fprintf(stderr, "error while creating compiler: %s\n", ast.as.error);
        return 1;
    }

    ResultType(voidPtr, charptr) result = compile(compiler.as.success, ast.as.success);
    if (result.error) {
        fprintf(stderr, "error while compiling: %s\n", result.as.error);
        return 1;
    }

    for (size_t i = 0; i < compiler.as.success->programIdx; i++) {
        printf("%s\n", cometInstructionToCStr(compiler.as.success->outputProgram[i]));
    }

    return 0;
}
