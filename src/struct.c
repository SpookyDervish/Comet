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
    printf("struct name: %s\n", structType->name);
    printf("field count: %d\n", structType->fieldCount);
    for (uint32_t i = 0; i < structType->fieldCount; i++) {
        printf("field name: %s (name) vs %s (looking for) (index %d)\n", structType->fieldNames[i], fieldName, i);
        if (strcmp(structType->fieldNames[i], fieldName) == 0) {
            printf("found it!\n");
            return i;
        }
    }

    printf("didn't find it :(\n");
    return -1;
}