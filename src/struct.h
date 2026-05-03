#ifndef STRUCT_H
#define STRUCT_H

#include "../include/list.h"
#include <stddef.h>
#include <string.h>
#include <llvm-c/Types.h>

typedef struct {
    LLVMTypeRef llvmType;
    unsigned index;
    char* name;
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