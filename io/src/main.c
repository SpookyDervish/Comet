#include <comet/cometlib.h>

static CometType cometTypeString = cometTypeVoid;

void ensureStringType() {
    if (cometTypeString.typeKind == COMET_VOID)
        cometTypeString = createArrayType(cometTypeSmall, 1, (bool[]){false}, (uint64_t[]){});
}

int64_t impl_print(int64_t* args, CometVM* vm) {
    ensureStringType();

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
                case 's': {
                    CometOperand strArr = deserializeValue(args[argIdx], cometTypeString);
                    char* str = (char*)cometArrayToCArray(strArr, cometTypeSmall);
                    printf("%s", str);
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

int64_t impl_getline(int64_t* args, CometVM* vm) {
    ensureStringType();

    char* userInput;
    size_t numChars;
    int64_t result = getline(&userInput, &numChars, stdin);
    if (result == -1) {
        return 0;
    }

    userInput[numChars] = 0;



    CometOperand strValue = CArrayToCometArray(userInput, numChars + 1, cometTypeSmall);

    int64_t serializedStr = serializeValue(strValue);

    return serializedStr;
}


on_import {
    if (cometTypeString.typeKind == COMET_VOID)
        cometTypeString = createArrayType(cometTypeSmall, 1, (bool[]){false}, (uint64_t[]){});

    cometDefineFunc(env, "print", cometTypeSmall, 1, true, cometTypeString);
    cometDefineFunc(env, "println", cometTypeSmall, 1, true, cometTypeString);
    cometDefineFunc(env, "getline", cometTypeString, 0, false);
}