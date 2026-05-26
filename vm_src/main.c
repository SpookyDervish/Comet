#include "vm.h"
#include "args.h"
#include "../lib/ansi.h"

#define VERSION_NUMBER "0.1.0"

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
}