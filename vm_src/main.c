#include "vm.h"
#include "args.h"
#include "../lib/ansi.h"
#include <stdio.h>

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

    ResultType(vmPtr, charptr) newVm = newCometVM(args.as.success.filePath);
    if (newVm.error) {
        fprintf(stderr, "error while loading file: %s\n", newVm.as.error);
        return 1;
    }

    ResultType(int, charptr) result = startVM(newVm.as.success);
    if (result.error) {
        fprintf(stderr, "error while executing: %s\n", result.as.error);
        return 1;
    }

    return result.as.success;
}