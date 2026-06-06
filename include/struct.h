#ifndef STRUCT_H
#define STRUCT_H

#include <stdbool.h>
#include <stdint.h>
#include "type.h"

typedef enum {
    FIELD_PRIVATE,
    FIELD_PROTECTED,
    FIELD_READ_ONLY,
    FIELD_PUBLIC,
} FieldAttribute;

// an instance of a struct
typedef struct CometSerializedFunc CometSerializedFunc;
typedef struct {
    int64_t* fields;
    CometSerializedFunc* vtable;
} CometObject;

FieldAttribute attribStringToFieldAttrib(char* str);
int32_t getFieldIndex(CometStruct* structType, char* fieldName);

#endif