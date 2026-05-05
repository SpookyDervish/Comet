#ifndef ARGS_H
#define ARGS_H

#include <argp.h>
#include <bits/types/error_t.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../include/error.h"

typedef struct {
    char* filePath;
    char* outputPath;
    bool outputLLVMIr;
    bool outputASM;
    int optimisation;
} CometArgs;

Result(CometArgs, charptr);

extern const struct argp_option options[];
extern const struct argp argp;
ResultType(CometArgs, charptr) parseArgs(int argc, char** argv);

#endif