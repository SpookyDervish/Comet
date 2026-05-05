#ifndef STRUCT_H
#define STRUCT_H

#include "../include/list.h"
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <llvm-c/Types.h>

typedef enum {
    FIELD_PRIVATE,
    FIELD_PROTECTED,
    FIELD_READ_ONLY,
} FieldAttribute;

typedef struct {
    LLVMTypeRef llvmType;
    unsigned index;
    char* name;
    bool isPointer;
    FieldAttribute attrib;
    bool isConst;
} StructField;

UseList(StructField);

typedef struct {
    LLVMTypeRef llvmType;
    List(StructField) fields;
    char* name;
} StructInfo;

UseList(StructInfo);

StructField* findField(StructInfo structInfo, char* fieldName);

#endif