#include "struct.h"

StructField* findField(StructInfo structInfo, char* fieldName) {
    for (size_t i = 0; i < structInfo.fields.count; i++) {
        StructField* field = get(structInfo.fields, i);

        if (strcmp(field->name, fieldName) == 0) {
            return field;
        }
    }

    return NULL;
}