#include "compiler_vm.h"

CometCVM* createCompilerVM(CometASTNode* ast) {
    CometCVM* newVm = calloc(sizeof(CometCVM), 1);
    newVm->ast = ast;

    return newVm;
}