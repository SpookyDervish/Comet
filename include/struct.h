#ifndef STRUCT_H
#define STRUCT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FIELD_PRIVATE,
    FIELD_PROTECTED,
    FIELD_READ_ONLY,
    FIELD_PUBLIC,
} FieldAttribute;

typedef struct {
    uint32_t startIdx;
    uint32_t argCount;
    uint32_t symbolIdx;
} CometMethod;


// the actual struct type
typedef struct {
    CometMethod** vtable;
    uint32_t numMethods;
    uint32_t fieldCount;
    char** fieldNames;
    char* name;
} CometStruct;

// an instance of a struct
typedef struct {
    int64_t* fields;
    uint32_t* vtable;
} CometObject;

FieldAttribute attribStringToFieldAttrib(char* str);
int32_t getFieldIndex(CometStruct* structType, char* fieldName);

#endif