#include <comet/cometlib.h>
#include <stdio.h>

CometOperand impl_subtract(CometOperand* args, CometVM* vm) {
    printf("we loaded a function and called it!\n");
    return cometValue(COMET_INT, args[0].imm.intVal - args[1].imm.intVal);
}

on_import {
    cometDefineFunc(env, "subtract", cometTypeInt, 2, cometTypeInt, cometTypeInt);
}