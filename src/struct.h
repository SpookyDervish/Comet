#ifndef STRUCT_H
#define STRUCT_H

#include "../include/list.h"
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

typedef enum {
    FIELD_PRIVATE,
    FIELD_PROTECTED,
    FIELD_READ_ONLY,
    FIELD_PUBLIC,
} FieldAttribute;

typedef struct {
    unsigned index;
    char* name;
    bool isPointer;
    FieldAttribute attrib;
    bool isConst;
} StructField;

UseList(StructField);

typedef List(StructField)* structFieldList;

typedef struct StructInfo StructInfo;
struct StructInfo {
    List(StructField) fields;
    char* name;
    StructInfo* parent;
};

UseList(StructInfo);

StructField* findField(StructInfo structInfo, char* fieldName);

typedef struct {
    char* fieldName;
    FieldAttribute attribType;
} FieldNameAttributePair;

extern const FieldNameAttributePair FIELD_STRING_MAP[];
FieldAttribute attribStringToFieldAttrib(char* keyword);

void printStruct(StructInfo structInfo);
void printStructFields(structFieldList fields);

#endif