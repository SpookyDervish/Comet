#include <comet/cometlib.h>

int64_t impl_print(int64_t* args, CometVM* vm) {
    CometOperand formatStringArray = deserializeValue(args[0], cometTypeString);
    char* formatString = (char*)cometArrayToCArray(formatStringArray, cometTypeSmall);

    printf("%s\n", formatString);

    return 0;
}

on_import {
    cometDefineFunc(env, "print", cometTypeSmall, 1, true, cometTypeString);
}