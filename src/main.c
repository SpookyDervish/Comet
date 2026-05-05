#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "args.h"
#include "util.h"
#include <stddef.h>
#include <stdio.h>



int main(int argc, char** argv) {
    // parse command line args
    ResultType(CometArgs, charptr) args = parseArgs(argc, argv);
    if (args.error) {
        fprintf(stderr, "error: %s\n", args.as.error);
        return 1;
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

    ResultType(cometCompilerPtr, charptr) compiler = createCompiler(parser.as.success);
    if (compiler.error) {
        fprintf(stderr, "error while creating compiler: %s\n", compiler.as.error);
        return 1;
    }

    ResultType(Nothing, charptr) result = compileAST(compiler.as.success, ast.as.success, args.as.success);
    if (result.error) {
        printf("Compiler error: %s\n", result.as.error);
        return 1;
    }

    return 0;
}