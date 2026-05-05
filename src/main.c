#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "util.h"
#include <stdlib.h>
#include <argp.h>

// define command line args
static struct argp_option options[] = {
    { "file", 'f', "FILE", 0, "input file to compile", 0 },
    { "output", 'o', "output", 0, "output path", 0 },
    { "llvm", 'l', "llvm", 0, "output llvm IR instead of an object file", 0 },
    { "asm", 'a', "asm", 0, "output assembly instead of an object file", 0 },
    { 0 }
};

typedef struct {
    char* filePath;
    char* outputPath;
    bool outputLLVMIr;
    bool outputASM;
} CometArgs;

static error_t parseCommandLineArgs(int key, char* arg, struct argp_state* state) {
    CometArgs* args = state->input;

    switch (key) {
        case 'o':
            args->outputPath = arg;
            break;

        case ARGP_KEY_ARG:
            if (state->arg_num == 0) {
                args->filePath = arg;
            } else {
                argp_usage(state); // too many args
            }
            break;

        case ARGP_KEY_END:
            if (state->arg_num < 1) {
                argp_usage(state);
            }
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp = { options, parseCommandLineArgs, NULL, NULL };

int main(int argc, char** argv) {
    // parse command line args
    CometArgs args = {0};
    argp_parse(&argp, argc, argv, 0, 0, &args);

    if (args.outputASM && args.outputLLVMIr) {
        printf("error: can't output both LLVM IR and assembly!\n");
    }

    // compile file
    char* source = getFileContents(args.filePath);

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

    ResultType(cometCompilerPtr, charptr) compiler = createCompiler(parser.as.success);
    if (compiler.error) {
        printf("error while creating compiler: %s\n", compiler.as.error);
    }

    ResultType(Nothing, charptr) result = compileAST(compiler.as.success, ast.as.success, args.outputPath, args.outputLLVMIr, args.outputASM);
    if (result.error) {
        printf("Compiler error: %s\n", result.as.error);
        exit(1);
    }
}