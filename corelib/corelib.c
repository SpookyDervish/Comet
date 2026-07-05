#include <comet/cometlib.h>

ResultType(int64_t, objectPtr) impl_Exception_INIT(int64_t* args, CometVM* vm) {
    CometObject* self = (CometObject*)args[0];

    self->fields[0] = cometSerializeString("Exception");
    self->fields[1] = args[1];

    return Success(int64_t, objectPtr, (int64_t)self);
}

void defineExceptionStruct(CometEnvironment* env, CometTypeMap* typeMap) {
    CometStruct* exceptionStruct = cometDefineStruct(env, "Exception", NULL);

    List(StructField) fields = newList(StructField);
    append(fields, cometCreateField("name", cometTypeString, FIELD_READ_ONLY));
    append(fields, cometCreateField("message", cometTypeString, FIELD_READ_ONLY));

    List(cometFuncPtr) methods = newList(cometFuncPtr);

    cometSetStructFieldsAndMethods(exceptionStruct, fields, methods);
    cometDefineConstructor(env, exceptionStruct, 1, false, cometTypeString);

    CometType exceptionType = {
        .typeKind = COMET_STRUCT,
        .structType = exceptionStruct
    };
    defineType(typeMap, "Exception", exceptionType);
}

on_import {
    defineExceptionStruct(env, typeMap);
}