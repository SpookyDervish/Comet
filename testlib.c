#include <comet/cometlib.h>

ResultType(int64_t, objectPtr) impl_set(int64_t* args, CometVM* vm) {
    CometObject* self = (CometObject*)args[1];

    self->fields[0] = args[0];

    return Success(int64_t, objectPtr, 0);
}

ResultType(int64_t, objectPtr) impl_Box_INIT(int64_t* args, CometVM* vm) {
    CometObject* self = (CometObject*)args[0];

    self->fields[0] = args[1];

    return Success(int64_t, objectPtr, (int64_t)self);
}

on_import {
    List(charptr) genericTypeNames = newList(charptr);
    append(genericTypeNames, "T");

    CometType genericType = cometGenericType("T");

    CometStruct* boxStruct = cometDefineGenericStruct(env, "Box", NULL, genericTypeNames);

    List(cometFuncPtr) methods = newList(cometFuncPtr);
    append(methods, cometDefineMethod(env, "set", boxStruct, cometTypeVoid, 1, false, genericType));
    printf("%d\n", methods.pointer[0]->argCount);

    List(StructField) fields = newList(StructField);

    cometDefineConstructor(env, boxStruct, 1, false, genericType);
    cometSetStructFieldsAndMethods(boxStruct, fields, methods);
}