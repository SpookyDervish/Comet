#include "args.h"
#include <argp.h>

// define command line args
const struct argp_option options[] = {
    { "file", 'f', "FILE", 0, "input file to compile", 0 },
    { "version", 'v', 0, 0, "display version number and quit", 0 },
    { 0 }
};

int parseCommandLineArgs(int key, char* arg, struct argp_state* state) {
    CometArgs* args = state->input;

    switch (key) {
        case 'v':
            args->showVersion = true;
            break;

        case ARGP_KEY_ARG:
            if (state->arg_num == 0) {
                args->filePath = arg;
            } else {
                argp_usage(state); // too many args
            }
            break;

        case ARGP_KEY_END:
            if (state->arg_num < 1 && !args->showVersion) {
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
    };
    argp_parse(&argp, argc, argv, 0, 0, &args);

    if (args.showVersion) {
        return Success(CometArgs, charptr, args);
    }

    if (!args.filePath) {
        return Error(CometArgs, charptr, "no input file specified\n");
    }

    return Success(CometArgs, charptr, args);
}