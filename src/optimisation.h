#ifndef OPTIMISATION_H
#define OPTIMISATION_H

#define MAX_OPTIMISE_PASSES 10

#include "compiler.h"
#include "parser.h"
#include "constant_env.h"
#include <math.h>
#include "../include/type.h"

void runOptimisations(CometCompiler* c, CometASTNode* ast);

#endif