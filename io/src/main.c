#include <comet/cometlib.h>

int64_t impl_print(int64_t* args, CometVM* vm) {
    CometOperand formatStringArray = deserializeValue(args[0], cometTypeString);
    char* formatString = (char*)cometArrayToCArray(formatStringArray, cometTypeSmall);

    size_t argIdx = 1;

    for (size_t i = 0; formatString[i] != 0; i++) {

        // look for '%'
        if (formatString[i] == '%') {
            switch (formatString[i+1]) {
                case 'n': {
                    printf("%ld", deserializeValue(args[argIdx], cometTypeBig).imm.bigVal);
                    break;
                }
                case 'f': {
                    printf("%f", deserializeValue(args[argIdx], cometTypeDouble).imm.doubleVal);
                    break;
                }
                case 'b': {
                    bool boolVal = deserializeValue(args[argIdx], cometTypeBool).imm.boolVal;
                    printf( boolVal ? "true" : "false");
                    break;
                }
                default: 
                    printf("%%%c", formatString[i+1]);
                    break;
            }
            i++;
            argIdx++;
        } else {
            putchar(formatString[i]);
        }

    }

    return 0;
}

int64_t impl_println(int64_t* args, CometVM* vm) {
    int64_t returnVal = impl_print(args, vm);
    putchar('\n');
    return returnVal;
}


on_import {
    cometDefineFunc(env, "print", cometTypeSmall, 1, true, cometTypeString);
    cometDefineFunc(env, "println", cometTypeSmall, 1, true, cometTypeString);
}