#ifndef OPTIMISATION_H
#define OPTIMISATION_H

#define MAX_OPTIMISE_PASSES 100

#include "compiler.h"
#include "parser.h"
#include "constant_env.h"
#include <math.h>
#include "../include/type.h"
#include "../lib/ansi.h"

void runOptimisations(CometCompiler* c, CometASTNode* ast, bool showPasses);

#endif