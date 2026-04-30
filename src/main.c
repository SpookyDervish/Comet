#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "util.h"
#include <stdlib.h>

int main() {
    char* source = getFileContents("test.cmt");

    ResultType(CometLexer, charptr) lexer = newLexer(source);
    ResultType(tokenList, charptr) tokens = lex(&lexer.as.success);

    if (tokens.error) {
        printf("lexer error: %s\n", tokens.as.error);
        exit(1);
    }
    
    ResultType(parserPtr, charptr) parser = newParser(tokens.as.success);
    if (parser.error) {
        printf("error while creating parser: %s\n", parser.as.error);
        exit(1);
    }

    ResultType(astNodePtr, charptr) ast = buildAST(parser.as.success);
    if (ast.error) {
        printf("error while building ast: %s\n", ast.as.error);
        exit(1);
    }

    printNode(ast.as.success);

    ResultType(cometCompilerPtr, charptr) compiler = createCompiler(parser.as.success);
    if (compiler.error) {
        printf("error while creating compiler: %s\n", compiler.as.error);
    }

    ResultType(Nothing, charptr) result = compileAST(compiler.as.success, ast.as.success, "output.llvm");
    if (result.error) {
        printf("Compiler error: %s\n", result.as.error);
        exit(1);
    }
}