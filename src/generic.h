#ifndef GENERIC_H
#define GENERIC_H

#include "../include/struct.h"
#include "ast.h"

typedef CometStruct* cometStructPtr;

typedef struct {
    char* name;
    CometASTNode* structDefNode;
} GenericStructDef;

UseList(CometType);
typedef struct {
    CometStruct* structType; // the result of the generic. e.g: Box_int
    List(CometType) genericTypes; // the types of all the generics. e.g: genericTypes[0] = T, T = int
    char* baseStructName; // the name of the base generic. e.g: Box
    GenericStructDef structDef; // ast node of struct def
} CachedGenericStruct;

UseList(GenericStructDef);
UseList(cometStructPtr);
UseList(CachedGenericStruct);

#endif