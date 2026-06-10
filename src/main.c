#include "compiler.h"
#include "inst.h"
#include "lexer.h"
#include "parser.h"
#include "args.h"
#include "util.h"
#include <stddef.h>
#include <stdio.h>
#include "../lib/ansi.h"


#define VERSION_NUMBER "0.5.0"


int main(int argc, char** argv) {
    // parse command line args
    ResultType(CometArgs, charptr) args = parseArgs(argc, argv);
    if (args.error) {
        fprintf(stderr, "error: %s\n", args.as.error);
        return 1;
    }

    // print version if we want
    if (args.as.success.showVersion) {
        printf("CometC - Version %s\n", VERSION_NUMBER);
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

    printNode(ast.as.success);
    printf("\n");

    ResultType(cometCompilerPtr, charptr) compiler = newCompiler();
    if (compiler.error) {
        freeNode(ast.as.success);
        fprintf(stderr, "error while creating compiler: %s\n", ast.as.error);
        return 1;
    }

    ResultType(CometOperand, charptr) compileResult = compile(compiler.as.success, ast.as.success);
    if (compileResult.error) {
        freeNode(ast.as.success);
        fprintf(stderr, "error while compiling: %s\n", compileResult.as.error);
        return 1;
    }

    ResultType(voidPtr, charptr) writeSuccess = outputToFile(compiler.as.success, args.as.success.outputPath);
    if (writeSuccess.error) {
        freeNode(ast.as.success);
        fprintf(stderr, "error while writing output: %s\n", writeSuccess.as.error);
        return 1;
    }

    freeNode(ast.as.success);

    return 0;
}