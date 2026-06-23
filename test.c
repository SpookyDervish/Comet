#include "include/cometlib.h"

int64_t impl_Foo_add(int64_t* args, CometVM* vm) {
    CometOperand a = deserializeValue(args[0], cometTypeInt);
    CometOperand b = deserializeValue(args[1], cometTypeInt);

    int result = a.imm.intVal + b.imm.intVal;
    return result;
}

int64_t impl_Foo_INIT(int64_t* args, CometVM* vm) {
    cometSetField((CometObject*)args[0], 0, args[1]);
    return args[0];
}

on_import {

    List(StructField) fields = newList(StructField);
    StructField field = (StructField){"test", cometTypeInt};
    append(fields, field);

    List(cometFuncPtr) methods = newList(cometFuncPtr);
    CometFunction* firstFunc = cometDefineFunc(env, "Foo_add", cometTypeInt, 2, false, true, cometTypeInt, cometTypeInt);
    append(methods, firstFunc);

    CometStruct* fooStruct = cometDefineStruct(env, "Foo", fields, methods);

    cometDefineConstructor(env, fooStruct, 1, false, cometTypeInt);
}