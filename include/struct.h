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

// the actual struct type
struct CometStruct {
    CometMethod** vtable;
    uint32_t numMethods;
    uint32_t fieldCount;
    char** fieldNames;
    CometType* fieldTypes;
    char* name;
    CometStruct* parent;

    // if this struct is a generic struct (not an instance of a generic) then this number is the number of generic types the struct takes.
    // e.g: in the case "struct Foo <T, T2> {...}" numGenericTypes would be 2
    uint8_t numGenericTypes; 
    char** genericTypeNames;

    // number of types given to the generic struct that created this instance. e.g: in the case "struct Box <T> {...}" if we create
    // an instance of Box<int> then numGivenGenericTypes = 1 and givenGenericTypes[0] = int
    uint8_t numGivenGenericTypes;
    CometType* givenGenericTypes; 
    
};

// an instance of a struct
typedef struct CometSerializedFunc CometSerializedFunc;
typedef struct {
    int64_t* fields;
    uint32_t* vtable;
} CometObject;

FieldAttribute attribStringToFieldAttrib(char* str);
int32_t getFieldIndex(CometStruct* structType, char* fieldName);

#endif