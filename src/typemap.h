#ifndef TYPEMAP_H
#define TYPEMAP_H

#include <uthash.h>
#include "../include/type.h"

typedef struct {
    char* name;
    CometType type;
    UT_hash_handle hh;
} CometTypeMapEntry;

typedef struct CometTypeMap CometTypeMap;
struct CometTypeMap {
    CometTypeMap* parent;
    CometTypeMapEntry* entries;
};


CometTypeMap* newTypemap(CometTypeMap* parent);
CometTypeMap* destroyTypeMap(CometTypeMap* typeMap);
bool defineType(CometTypeMap* typeMap, char* name, CometType type);
CometTypeMapEntry* lookupType(CometTypeMap* typeMap, char* typeName);

#endif