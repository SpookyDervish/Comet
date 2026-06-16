#include "compiler.h"
#include "inst.h"
#include "lexer.h"
#include "parser.h"
#include "args.h"
#include "util.h"
#include "error_message.h"
#include <stddef.h>
#include <stdio.h>
#include "../lib/ansi.h"


#define VERSION_NUMBER "0.6.0"


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
    char* filePath = args.as.success.filePath;
    char* source = getFileContents(filePath);

    CometLexer lexer = newLexer(source, filePath);
    ResultType(tokenList, ErrorMessage) tokens = lex(&lexer);

    if (tokens.error) {
        printErrorMessage(tokens.as.error);
        return 1;
    }

    ResultType(parserPtr, ErrorMessage) parser = newParser(tokens.as.success);
    if (parser.error) {
        printErrorMessage(parser.as.error);
        return 1;
    }

    ResultType(astNodePtr, ErrorMessage) ast = buildAST(parser.as.success);
    if (ast.error) {
        printErrorMessage(ast.as.error);
        return 1;
    }

    ResultType(cometCompilerPtr, ErrorMessage) compiler = newCompiler(filePath, source);
    if (compiler.error) {
        freeNode(ast.as.success);
        printErrorMessage(compiler.as.error);
        return 1;
    }

    ResultType(CometOperand, ErrorMessage) compileResult = compile(compiler.as.success, ast.as.success);
    if (compileResult.error) {
        freeNode(ast.as.success);
        printErrorMessage(compileResult.as.error);
        return 1;
    }

    ResultType(voidPtr, ErrorMessage) writeSuccess = outputToFile(compiler.as.success, args.as.success.outputPath);
    if (writeSuccess.error) {
        freeNode(ast.as.success);
        printErrorMessage(writeSuccess.as.error);
        return 1;
    }

    freeNode(ast.as.success);

    return 0;
}