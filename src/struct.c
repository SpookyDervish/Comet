#include "../include/struct.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

FieldAttribute attribStringToFieldAttrib(char* str) {
    if (strcmp(str, "private") == 0) {
        return FIELD_PRIVATE;
    } else if (strcmp(str, "protected") == 0) {
        return FIELD_PROTECTED;
    } else if (strcmp(str, "readonly") == 0) {
        return FIELD_READ_ONLY;
    } else {
        return FIELD_PUBLIC;
    }
}

int32_t getFieldIndex(CometStruct* structType, char* fieldName) {
    for (uint32_t i = 0; i < structType->fieldCount; i++) {
        if (strcmp(structType->fieldNames[i], fieldName) == 0) {
            return i;
        }
    }

    return -1;
}