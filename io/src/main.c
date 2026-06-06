#include "../../include/cometlib.h"

CometOperand fooFunc(List(CometOperand) args, CometVM* vm) {
    return cometValue(COMET_INT, 123);
}

on_import {
    cometDefineFunc(env, "foo", fooFunc, cometTypeInt, 0);
}