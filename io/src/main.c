#include "../../include/cometlib.h"

CometOperand impl_subtract(CometOperand* args, CometVM* vm) {
    return cometValue(COMET_INT, args[0].imm.intVal - args[1].imm.intVal);
}

on_import {
    cometDefineFunc(env, "print", cometTypeVoid, 1, true, cometTypeBig);
}