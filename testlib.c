#include <comet/cometlib.h>
#include <stdio.h>

ResultType(int64_t, objectPtr) impl_Test_INIT(int64_t* args, CometVM* vm) {
    printf("constructor\n");
    return Success(int64_t, objectPtr, args[0]);
}

ResultType(int64_t, objectPtr) impl_Test_DESTROY(int64_t* args, CometVM* vm) {
    printf("destructor\n");
    return Success(int64_t, objectPtr, args[0]);
}

on_import {
    CometStruct* testStruct = cometDefineStruct(env, "Test", NULL);

    List(StructField) fields = newList(StructField);
    List(cometFuncPtr) methods = newList(cometFuncPtr);

    cometDefineConstructor(env, testStruct, 0, false);
    cometDefineDestructor(env, testStruct);
}