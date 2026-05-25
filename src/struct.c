#include "struct.h"
#include <llvm-c/Core.h>
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

void printStructFields(structFieldList fields) {

    for (size_t i = 0; i < fields->count; i++) {
        StructField field = *get((*fields), i);

        printf("  field: %s\n \
    index: %d\n \
    access: %d\n \
    is const? %d\n \
    is pointer? %d\n"
    , field.name, field.index, field.attrib, field.isConst, field.isPointer);
    }
}

void printStruct(StructInfo structInfo) {
    printf("struct %s {\n", structInfo.name);
    
    printStructFields(&structInfo.fields);
}

FieldAttribute attribStringToFieldAttrib(char* keyword) {
    for (size_t i = 0; i < sizeof(FIELD_STRING_MAP)/sizeof(FIELD_STRING_MAP[0]); i++) {
        if (strcmp(FIELD_STRING_MAP[i].fieldName, keyword) == 0) {
            return FIELD_STRING_MAP[i].attribType;
        }
    }

    return FIELD_PUBLIC;
}