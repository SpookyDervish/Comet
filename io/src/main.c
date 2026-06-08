#include <comet/cometlib.h>
#include <stdio.h>

CometOperand impl_foo(List(CometOperand) args, CometVM* vm) {
    printf("we loaded a function and called it!\n");
    return cometValue(COMET_INT, 123);
}

on_import {
    cometDefineFunc(env, "foo", cometTypeInt, 0);
}