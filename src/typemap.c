#include "typemap.h"

CometTypeMap* newTypemap(CometTypeMap* parent) {
    CometTypeMap* newMap = malloc(sizeof(CometTypeMap));
    if (!newMap)
        return NULL;

    newMap->parent = parent;
    newMap->entries = NULL;

    return newMap;
}

CometTypeMap* destroyTypeMap(CometTypeMap* typeMap) {
    CometTypeMap* parent = typeMap->parent;
    free(typeMap);
    return parent;
}

bool defineType(CometTypeMap* typeMap, char* name, CometType type) {
    CometTypeMapEntry* entry;
    HASH_FIND_STR(typeMap->entries, name, entry);

    if (entry != NULL) {
        return false;
    }

    entry = malloc(sizeof(CometTypeMapEntry));
    entry->name = strdup(name);
    entry->type = type;

    HASH_ADD_KEYPTR(hh, typeMap->entries, entry->name, strlen(entry->name), entry);
    return true;
}

CometTypeMapEntry* lookupType(CometTypeMap* typeMap, char* typeName) {
    CometTypeMapEntry* entry;
    HASH_FIND_STR(typeMap->entries, typeName, entry);

    if (entry != NULL) {
        return entry;
    }

    if (typeMap->parent) {
        return lookupType(typeMap, typeName);
    }

    return NULL;
}