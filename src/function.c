#include "../include/function.h"
#include "inst.h"

CometFunction* copyFunction(CometCompiler* c, CometFunction* func, char* newName) {
    CometOperand funcSymbol = buildFunction(c, newName, func->argCount, func->returnType, func->argTypes, func->isVarArgs, func->isMethod, func->isExternal, func->libIdx);
    return c->functions[funcSymbol.symbolIdx];
}