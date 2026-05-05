#include "args.h"
#include "compiler.h"

// define command line args
const struct argp_option options[] = {
    { "file", 'f', "FILE", 0, "input file to compile", 0 },
    { "output", 'o', "OUTPUT", 0, "output path", 0 },
    { "llvm", 'l', "LLVM", 0, "output llvm IR instead of an object file", 0 },
    { "asm", 'a', "ASM", 0, "output assembly instead of an object file", 0 },
    { "optimisation", 'O', "OPTIMISATION", 0, "the level of optimisation (0 - 3, default is 2)", 0 },
    { 0 }
};

error_t parseCommandLineArgs(int key, char* arg, struct argp_state* state) {
    CometArgs* args = state->input;

    switch (key) {
        case 'o':
            args->outputPath = arg;
            break;
        
        case 'O':
            args->optimisation = atoi(arg);
            
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

const struct argp argp = { options, parseCommandLineArgs, NULL, NULL, NULL, NULL, NULL };

ResultType(CometArgs, charptr) parseArgs(int argc, char** argv) {
    CometArgs args = {
        .filePath = NULL,
        .outputPath = NULL,
        .outputASM = false,
        .outputLLVMIr = false,
        .optimisation = 2
    };
    argp_parse(&argp, argc, argv, 0, 0, &args);


    // check args
    if (args.outputASM && args.outputLLVMIr) {
        return Error(CometArgs, charptr, "can't output both LLVM IR and assembly!\n");
    }

    if (args.optimisation < 0) {
        return Error(CometArgs, charptr, "optimisation level can't be less than 0\n");
    } else if (args.optimisation > 3) {
        return Error(CometArgs, charptr, "optimisation level can't be higher than 3\n");
    }

    if (!args.filePath) {
        return Error(CometArgs, charptr, "no input file specified\n");
    }
    if (!args.outputPath) {
        return Error(CometArgs, charptr, "no output file specified\n");
    }

    return Success(CometArgs, charptr, args);
}