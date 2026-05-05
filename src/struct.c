#include "struct.h"
#include <stddef.h>
#include <string.h>

StructField* findField(StructInfo structInfo, char* fieldName) {
    for (size_t i = 0; i < structInfo.fields.count; i++) {
        StructField* field = get(structInfo.fields, i);

        if (strcmp(field->name, fieldName) == 0) {
            return field;
        }
    }

    return NULL;
}

const FieldNameAttributePair FIELD_STRING_MAP[] = {
    {"private", FIELD_PRIVATE},
    {"protected", FIELD_PROTECTED},
    {"readonly", FIELD_READ_ONLY},
};


FieldAttribute attribStringToFieldAttrib(char* keyword) {
    for (size_t i = 0; i < sizeof(FIELD_STRING_MAP)/sizeof(FIELD_STRING_MAP[0]); i++) {
        if (strcmp(FIELD_STRING_MAP[i].fieldName, keyword) == 0) {
            return FIELD_STRING_MAP[i].attribType;
        }
    }

    return FIELD_PUBLIC;
}