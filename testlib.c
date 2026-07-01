#include "include/cometlib.h"

ResultType(int64_t, objectPtr) impl_Pointer_INIT(int64_t* args, CometVM* vm) {
    CometObject* self = (CometObject*)args[0];
    printf("%p\n", self);
    printf("%p\n", args[1]);
    printf("%p\n", args[2]);

    self->fields[0] = args[1];
    printf("a\n");
    self->fields[1] = args[2];
    printf("b\n");


    return Success(int64_t, objectPtr, (int64_t)self);
}

on_import {
    CometType genericType = cometGenericType("T");

    List(charptr) genericTypeNames = newList(charptr);
    append(genericTypeNames, "T");

    CometStruct* ptrStruct = cometDefineGenericStruct(env, "Pointer", NULL, genericTypeNames);

    List(StructField) fields = newList(StructField);
    append(fields, cometCreateField("value", genericType));
    append(fields, cometCreateField("size", cometTypeBig));

    List(cometFuncPtr) methods = newList(cometFuncPtr);
    //append(methods, cometDefineMethod(env, "resize", ptrStruct, cometTypeVoid, 1, false, cometTypeBig));

    cometSetStructFieldsAndMethods(ptrStruct, fields, methods);
    cometDefineConstructor(env, ptrStruct, 2, false, genericType, cometTypeBig);
}