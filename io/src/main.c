#include <comet/cometlib.h>
#include <stdio.h>

CometOperand impl_foo(CometOperand* args, CometVM* vm) {
    printf("we loaded a function and called it!\n");
    return cometValue(COMET_INT, args[0]);
}

on_import {
    cometDefineFunc(env, "foo", cometTypeInt, 1, cometTypeInt);
}