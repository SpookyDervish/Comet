#include <comet/cometlib.h>
#include <stdio.h>

CometOperand fooFunc(List(CometOperand) args, CometVM* vm) {
    printf("we loaded a function and called it!\n");
    return cometValue(COMET_INT, 123);
}

on_import {
    cometDefineFunc(env, "foo", fooFunc, cometTypeInt, 0);
}