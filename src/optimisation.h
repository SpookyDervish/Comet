#ifndef OPTIMISATION_H
#define OPTIMISATION_H

#include "compiler.h"
#include "parser.h"
#include <math.h>
#include "../include/type.h"

void runOptimisations(CometCompiler* c, CometASTNode* ast);

#endif