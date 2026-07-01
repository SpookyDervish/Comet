#include "../include/cometlib.h"
#include "../include/environment.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static CometStruct* exceptStruct = NULL;

int8_t cometArgSmall(int64_t argVal) {
    return cometDeserializeValue(argVal, cometTypeSmall).imm.smallVal;
}
int32_t cometArgInt(int64_t argVal) {
    return cometDeserializeValue(argVal, cometTypeInt).imm.intVal;
}
int64_t cometArgBig(int64_t argVal) {
    return cometDeserializeValue(argVal, cometTypeBig).imm.bigVal;
}
float cometArgFloat(int64_t argVal) {
    return cometDeserializeValue(argVal, cometTypeFloat).imm.floatVal;
}
double cometArgDouble(int64_t argVal) {
    return cometDeserializeValue(argVal, cometTypeDouble).imm.doubleVal;
}
bool cometArgBool(int64_t argVal) {
    return cometDeserializeValue(argVal, cometTypeBool).imm.boolVal;
}
char* cometArgString(int64_t argVal) {
    CometOperand stringArr = cometDeserializeValue(argVal, cometTypeString);
    return cometArrayToCArray(stringArr, cometTypeSmall);
}
int64_t cometSerializeString(char* cString) {
    CometOperand cometArray = CArrayToCometArray(cString, strlen(cString) + 1, cometTypeSmall);
    return cometSerializeValue(cometArray);
}

int64_t Exception_INIT(int64_t* args, CometVM* vm) {
    CometObject* exception = (CometObject*)args[0];
    cometSetField(exception, 0, args[1]);
    cometSetField(exception, 1, args[2]);
    return (int64_t)exception;
}

CometStruct* cometGetExceptionStruct(CometEnvironment* env) {
    if (exceptStruct != NULL)
        return exceptStruct;

    CometStruct* exceptStruct = cometDefineStruct(env, "Exception", NULL);

    List(StructField) fields = newList(StructField);
    StructField typeField = { .name = "type",    .type = cometTypeString };
    StructField msgField  = { .name = "message", .type = cometTypeString };

    List(cometFuncPtr) methods = newList(cometFuncPtr);

    cometSetStructFieldsAndMethods(exceptStruct, fields, methods);
    cometDefineConstructor(env, exceptStruct, 2, false, cometTypeString, cometTypeString);

    return exceptStruct;
}

void cometSetStructFieldsAndMethods(CometStruct* cometStruct, List(StructField) fields, List(cometFuncPtr) methods) {
    size_t fieldCount  = cometStruct->parent == NULL ? fields.count : fields.count + cometStruct->parent->fieldCount;
    size_t methodCount = cometStruct->parent == NULL ? methods.count : methods.count + cometStruct->parent->numMethods;

    

    char** fieldNames = calloc(fieldCount, sizeof(char*));
    CometType* fieldTypes = calloc(fieldCount, sizeof(CometType));

    CometFunction** methodsArr = calloc(methodCount, sizeof(CometFunction*));
    size_t methodIdx = 0;
    size_t fieldIdx = 0;

    if (cometStruct->parent != NULL) {
        memcpy(methodsArr, cometStruct->parent->vtable, sizeof(CometMethod*) * cometStruct->parent->numMethods);

        memcpy(fieldNames, cometStruct->parent->fieldNames, sizeof(char*) * cometStruct->parent->fieldCount);
        memcpy(fieldTypes, cometStruct->parent->fieldTypes, sizeof(CometType) * cometStruct->parent->fieldCount);
    }

    for (methodIdx = methodIdx; methodIdx < methods.count; methodIdx++) {
        methodsArr[methodIdx] = *get(methods, methodIdx);
    }

    for (fieldIdx = fieldIdx; fieldIdx < fields.count; fieldIdx++) {
        StructField field = *get(fields, fieldIdx);
        fieldNames[fieldIdx] = strdup(field.name); // we strdup it or else it might go out of scope and become invalid
        fieldTypes[fieldIdx] = field.type;
    }

    cometStruct->fieldNames = fieldNames;
    cometStruct->fieldTypes = fieldTypes;
    cometStruct->fieldCount = fields.count;
    cometStruct->numMethods = methods.count;
    cometStruct->vtable = (CometMethod**)methodsArr;
}

CometStruct* cometDefineStruct(CometEnvironment* env, char* name, CometStruct* parent) {
    CometStruct* newStruct = malloc(sizeof(CometStruct)); 

    

    name = strdup(name);

    
    
    *newStruct = (CometStruct){
        .name = name,
        .fieldNames = NULL,
        .fieldTypes = NULL,
        .fieldCount = 0,
        .numMethods = 0,
        .vtable = NULL,
        .parent = parent
    };

    CometType structType = {
        .typeKind = COMET_STRUCT,
        .structType = newStruct
    };

    CometOperand structVal = {
        .type = CO_IMMEDIATE,
        .imm.typeKind = COMET_TYPE,
        .imm.typeVal = structType
    };

    CometType typeType = { // bru
        .typeKind = COMET_TYPE,
    };

    if (env)
        defineVar(env, name, RECORD_LOCAL, structVal, typeType, false);

    return newStruct;
}

CometFunction* cometDefineFunc(
    CometEnvironment* env,
    char* name,
    CometType returnType,
    uint32_t numArgs,
    bool isVarArgs,
    ...
) {
    CometFunction* func = malloc(sizeof(CometFunction));
    memcpy(func->name, name, 32);
    func->argCount = numArgs;
    func->isMethod = false;
    func->returnType = returnType;
    func->blockIdx = 0;
    func->isExternal = true;
    func->isVarArgs = isVarArgs;
    
    CometType type = {
        .typeKind = COMET_FUNCTION,
    };
    type.functionType = func;

    CometOperand funcVal = {
        .type = CO_IMMEDIATE,
        .imm = {
            .typeKind = COMET_FUNCTION,
            .bigVal = (int64_t)func
        }
    };

    va_list args;
    va_start(args, isVarArgs);

    CometType* argTypes = numArgs > 0 ? calloc(numArgs, sizeof(CometType)) : NULL;
    for (size_t i = 0; i < numArgs; i++) {
        argTypes[i] = va_arg(args, CometType);
    }

    func->argTypes = argTypes;

    va_end(args);

    if (env)
        defineVar(env, name, RECORD_LOCAL, funcVal, type, false);
    
    return func;
}

CometFunction* cometDefineMethod(
    CometEnvironment* env,
    char* name,
    CometStruct* cometStruct,
    CometType returnType,
    uint32_t numArgs,
    bool isVarArgs,
    ...
) {
    CometFunction* func = malloc(sizeof(CometFunction));
    memcpy(func->name, name, 32);
    func->argCount = numArgs + 1;
    func->isMethod = true;
    func->returnType = returnType;
    func->blockIdx = 0;
    func->isExternal = true;
    func->isVarArgs = isVarArgs;
    
    CometType type = {
        .typeKind = COMET_FUNCTION,
    };
    type.functionType = func;

    CometOperand funcVal = {
        .type = CO_IMMEDIATE,
        .imm = {
            .typeKind = COMET_FUNCTION,
            .bigVal = (int64_t)func
        }
    };

    va_list args;
    va_start(args, isVarArgs);

    CometType* argTypes = calloc(numArgs + 1, sizeof(CometType));
    argTypes[0] = (CometType){ .typeKind = COMET_STRUCT, .structType = cometStruct };
    for (size_t i = 0; i < numArgs; i++) {
        argTypes[i + 1] = va_arg(args, CometType);
    }

    func->argTypes = argTypes;

    va_end(args);

    if (env)
        defineVar(env, name, RECORD_LOCAL, funcVal, type, false);

    return func;
}

StructField cometCreateField(char* name, CometType type) {
    return (StructField){
        .name = strdup(name),
        .type = type
    };
}

void cometDefineConstructor(
    CometEnvironment* env,
    CometStruct* cometStruct,
    uint32_t numArgs,
    bool isVarArgs,
    ...
) {
    CometType structType = {
        .typeKind = COMET_STRUCT,
        .structType = cometStruct
    };

    char* constructorName = malloc(32);
    snprintf(constructorName, 32, "%s_INIT", cometStruct->name);

    CometFunction* func = malloc(sizeof(CometFunction));
    memcpy(func->name, constructorName, 32);
    func->argCount = numArgs + 1;
    func->isMethod = false;
    func->returnType = structType;
    func->blockIdx = 0;
    func->isExternal = true;
    func->isVarArgs = isVarArgs;
    
    CometType type = {
        .typeKind = COMET_FUNCTION,
        .functionType = func
    };

    CometOperand funcVal = {
        .type = CO_IMMEDIATE,
        .imm = {
            .typeKind = COMET_FUNCTION,
            .bigVal = (int64_t)func
        }
    };

    va_list args;
    va_start(args, isVarArgs);

    CometType* argTypes = calloc(numArgs + 1, sizeof(CometType));
    argTypes[0] = structType;
    for (size_t i = 0; i < numArgs; i++) {
        argTypes[i + 1] = va_arg(args, CometType);
    }

    func->argTypes = argTypes;

    va_end(args);

    if (env)
        defineVar(env, constructorName, RECORD_LOCAL, funcVal, type, false);
}

CometOperand cometValue(CometValueTypeKind valueType, ...) {
    va_list args;
    va_start(args, valueType);

    CometOperand newVal;

    switch (valueType) {
        case COMET_SMALL: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_SMALL,
                .imm.smallVal = (int8_t)va_arg(args, int)
            };
            break;
        case COMET_INT: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_INT,
                .imm.intVal = (int32_t)va_arg(args, int)
            };
            break;
        case COMET_BIG: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_BIG,
                .imm.bigVal = (int64_t)va_arg(args, int)
            };
            break;
        case COMET_FLOAT: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_FLOAT,
                .imm.floatVal = (float)va_arg(args, double)
            };
            break;
        case COMET_DOUBLE: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_DOUBLE,
                .imm.doubleVal = (double)va_arg(args, double)
            };
            break;
        case COMET_BOOL:
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_BOOL,
                .imm.boolVal = (bool)va_arg(args, int)
            };
            break;
        case COMET_STRUCT:
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_STRUCT,
                .imm.objectVal = (CometObject*)va_arg(args, CometObject*)
            };
            break;
        case COMET_FUNCTION:
            newVal = (CometOperand){
                .type = CO_SYMBOL,
                .symbolIdx = (uint32_t)va_arg(args, int)
            };
            break;
        case COMET_VOID: 
            newVal = (CometOperand){
                .type = CO_NONE
            };
            break;
        case COMET_TYPE: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_TYPE,
                .imm.typeVal = va_arg(args, CometType)
            };
            break;
        case COMET_MODULE:
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.typeKind = COMET_MODULE,
                .imm.moduleVal = va_arg(args, CometEnvironment*)
            };
            break;
    }

    va_end(args);
    return newVal;
}

CometType cometCreateArrayType(CometType elem, uint8_t dimensions, bool isFixedSize[], uint64_t fixedSize[]) {
    CometType* elemPtr = malloc(sizeof(CometType));
    if (!elemPtr) return cometTypeVoid;

    *elemPtr = elem;

    CometArrayType* arrayType = malloc(sizeof(CometArrayType));
    if (!arrayType) return cometTypeVoid;

    arrayType->elem = elemPtr;
    arrayType->dims = dimensions;
    for (size_t i = 0; i < dimensions; i++) {
        if (isFixedSize[i]) {
            arrayType->isFixedSize[i] = true;
            arrayType->fixedSize[i] = fixedSize[i];
        } else {
            arrayType->isFixedSize[i] = false;

        }
    }

    memcpy(arrayType->isFixedSize, isFixedSize, sizeof(bool) * dimensions);
    memcpy(arrayType->fixedSize, fixedSize, sizeof(uint64_t) * dimensions);
    
    return (CometType){
        .typeKind = COMET_ARRAY,
        .arrayType = arrayType
    };
}

inline void cometSetField(CometObject* object, uint32_t index, int64_t value) {
    object->fields[index] = value;
}

CometSerializedStruct* cometVMGetStruct(CometVM* vm, char* structName) {
    for (size_t i = 0; i < vm->numStructs; i++) {
        if (strcmp(vm->structs[i].name, structName) == 0) {
            return &vm->structs[i];
        }
    }

    return NULL;
}

API_EXPORT CometObject* cometCreateObject(CometSerializedStruct* structType) {
    CometObject* obj = malloc(sizeof(CometObject));

    obj->fields = calloc(structType->numFields, sizeof(int64_t));
    obj->vtable = structType->vtable;

    return obj;
}

ResultType(int64_t, objectPtr) cometError(CometVM* vm, char* errorName, char* errorMessage) {
    CometSerializedStruct* exceptionStruct = cometVMGetStruct(vm, "Exception");
    CometObject* exceptionObj = cometCreateObject(exceptionStruct);

    exceptionObj->fields[0] = cometSerializeString(errorName);
    exceptionObj->fields[1] = cometSerializeString(errorMessage);

    return Error(int64_t, objectPtr, exceptionObj);
}