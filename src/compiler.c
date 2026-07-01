#include "compiler.h"
#include "ast.h"
#include "inst.h"
#include "lexer.h"
#include "token.h"
#include "parser.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h>


// -- HELPER METHODS -- //
char* typeToString(CometType type) {
    

    switch (type.typeKind) {
        case COMET_SMALL: return "small";
        case COMET_INT: return "int";
        case COMET_BIG: return "big";
        case COMET_FLOAT: return "float";
        case COMET_DOUBLE: return "double";
        case COMET_BOOL: return "bool";
        case COMET_VOID: return "void";

        case COMET_GENERIC: {
            char* buffer = malloc(64);
            if (!buffer)
                return NULL;

            snprintf(buffer, 64, "<generic: %s>", type.genericParamName);
            return buffer;
        }

        case COMET_FUNCTION: {
            int buffsize = 256;
            char* buffer = malloc(buffsize);
            if (!buffer)
                return NULL;

            int remaining = buffsize;
            int written = 0;

            CometFunction* f = type.functionType;

            written += snprintf(buffer + written, remaining, "%s(", f->name);
            remaining = buffsize - written;

            for (size_t i = 0; i < f->argCount; i++) {
                if (i >= f->argCount-1) // end of args

                    if (f->isVarArgs) // add dots if this function has variadic args
                        written += snprintf(buffer + written, remaining, "%s, ...) -> ", typeToString(f->argTypes[i]));
                    else
                        written += snprintf(buffer + written, remaining, "%s) -> ", typeToString(f->argTypes[i]));

                else // not end of args yet
                    written += snprintf(buffer + written, remaining, "%s, ", typeToString(f->argTypes[i]));
                remaining = buffsize - written;
            }

            written += snprintf(buffer + written, remaining, "%s", typeToString(f->returnType));
            remaining = buffsize - written;
            
            return buffer;
        }

        case COMET_ARRAY: {
            int buffSize = 128;

            char* buffer = malloc(buffSize);
            if (!buffer)
                return NULL;

            int written = snprintf(buffer, buffSize, "%s[", typeToString(*type.arrayType->elem));
            int remaining = buffSize - written;
    
            for (size_t i = 0; i < type.arrayType->dims; i++) {
                if (type.arrayType->isFixedSize[i]) {
                    written += snprintf(
                        buffer + written,
                        remaining,
                        "%lu", 
                        type.arrayType->fixedSize[i]
                    );
                } else {
                    written += snprintf(
                        buffer + written,
                        remaining,
                        "*"
                    );
                }
    
                if (i >= type.arrayType->dims - 1) {
    
                    written += snprintf(
                        buffer + written, 
                        remaining,
                        "]"
                    );
                    
                    
                } else {
                    written += snprintf(
                        buffer + written, 
                        remaining,
                        ", "
                    );
                }
    
                remaining = buffSize - written;
            }

            return buffer;
        }

        case COMET_STRUCT: {
            Estr buffer = CREATE_ESTR(type.structType->name);

            if (type.structType->numGivenGenericTypes > 0) {
                APPEND_ESTR(buffer, "<");
                for (size_t i = 0; i < type.structType->numGivenGenericTypes; i++) {
                    APPEND_ESTR(buffer, typeToString(type.structType->givenGenericTypes[i]));

                    if (i < type.structType->numGivenGenericTypes - 1) {
                        printf(", ");
                    }
                }
                APPEND_ESTR(buffer, ">");
            }

            return buffer.str;
        }

        default: return "unkown";
    }
}

bool methodIsGeneric(CometCompiler* c, CometMethod* method) {
    // function may return a generic
    if (method->returnType.typeKind == COMET_GENERIC)
        return true;

    // one of the function's args may be a generic type
    CometFunction* funcPtr = c->functions[method->symbolIdx];
    for (size_t argIdx = 0; argIdx < funcPtr->argCount; argIdx++) {

        if (funcPtr->argTypes[argIdx].typeKind == COMET_GENERIC)
            return true;

    }

    // nah isnt a generic
    return false;
}

CometType* resolveGenericType(char* genericValTypeName, List(GenericTypeMapping) resolvedGenericTypes) {
    for (size_t genericIdx = 0; genericIdx < resolvedGenericTypes.count; genericIdx++) {
        GenericTypeMapping* resolvedGeneric = get(resolvedGenericTypes, genericIdx);

        if (strcmp(resolvedGeneric->genericTypeName, genericValTypeName) == 0) {
            return &resolvedGeneric->newType;
        }
    }

    return NULL;
}

CometStruct* getGenericStruct(CometCompiler* c, CometStruct* cometStruct, List(GenericTypeMapping) resolvedGenericTypes) {
    // look for existing struct
    for (size_t i = 0; i < c->cachedGenerics.count; i++) {
        CachedGenericStruct currentStruct = *get(c->cachedGenerics, i);

        if (strcmp(currentStruct.structType->name, cometStruct->name) == 0) {

            bool found = true;

            for (size_t genericTypeIdx = 0; genericTypeIdx < currentStruct.genericTypes.count; genericTypeIdx++) {
                if (!typesAreEqual(get(currentStruct.genericTypes, genericTypeIdx)[genericTypeIdx], get(resolvedGenericTypes, genericTypeIdx)->newType)) {
                    found = false;
                    break;
                }
            }

            if (found) {
                return currentStruct.structType;
            }
        }
    }

    // if it doesnt exist, make a new one with the types
    Estr newStructName = CREATE_ESTR(cometStruct->name);
    for (size_t i = 0; i < resolvedGenericTypes.count; i++) {
        APPEND_ESTR(newStructName, "_");
        APPEND_ESTR(newStructName, typeToString(get(resolvedGenericTypes, i)->newType));
    }

    CometType* newFieldTypes = malloc(sizeof(CometType) * resolvedGenericTypes.count);

    // fill in generic fields
    for (size_t fieldIdx = 0; fieldIdx < cometStruct->fieldCount; fieldIdx++) {
        if (cometStruct->fieldTypes[fieldIdx].typeKind != COMET_GENERIC)
            continue; // dont replace type of a field that doesnt use a generic type

        newFieldTypes[fieldIdx] = *resolveGenericType(cometStruct->fieldTypes[fieldIdx].genericParamName, resolvedGenericTypes);
    }

    // fill in generic methods
    for (size_t methodIdx; methodIdx < cometStruct->numMethods; methodIdx++) {
        CometMethod* method = cometStruct->vtable[methodIdx];

        if (!methodIsGeneric(c, method)) 
            continue;

        if (method->returnType.typeKind == COMET_GENERIC)
            method->returnType = *resolveGenericType(method->returnType.genericParamName, resolvedGenericTypes);

        CometFunction* funcPtr = c->functions[method->symbolIdx];

        for (size_t argIdx = 0; argIdx < method->argCount; argIdx++) {
            CometType genericArgType = funcPtr->argTypes[argIdx];

            if (genericArgType.typeKind == COMET_GENERIC) {
                funcPtr->argTypes[argIdx] = *resolveGenericType(genericArgType.genericParamName, resolvedGenericTypes);
            }
        }
        
    }

    CometType* givenGenericTypes = malloc(sizeof(CometType) * resolvedGenericTypes.count);
    for (size_t i = 0; i < resolvedGenericTypes.count; i++) {
        givenGenericTypes[i] = (*get(resolvedGenericTypes, i)).newType;
    }

    CometStruct* newStruct = malloc(sizeof(CometStruct));
    newStruct->name = cometStruct->name;
    newStruct->fieldTypes = newFieldTypes;
    newStruct->fieldNames = cometStruct->fieldNames;
    newStruct->fieldCount = cometStruct->fieldCount;
    newStruct->vtable = cometStruct->vtable;
    newStruct->numMethods = cometStruct->numMethods;
    newStruct->genericTypeNames = NULL;
    newStruct->numGenericTypes = 0;
    newStruct->givenGenericTypes = givenGenericTypes;
    newStruct->numGivenGenericTypes = resolvedGenericTypes.count;
    newStruct->parent = cometStruct->parent;

    CachedGenericStruct newGeneric = {
        .structType = newStruct,
        .genericTypes = newFieldTypes
    };

    append(c->structs, newStruct);
    append(c->cachedGenerics, newGeneric);

    return newStruct;
}

bool isAssignmentOperator(CometASTNode* expr) {
    CometTokenType assignmentOps[] = {
        CT_PLUS_EQ,
        CT_MINUS_EQ,
        CT_TIMES_EQ,
        CT_DIVIDE_EQ,
        CT_MOD_EQ,
        CT_POW_EQ
    };

    for (size_t i = 0; i < sizeof(assignmentOps)/sizeof(assignmentOps[0]); i++) {
        if (expr->data.AST_INFIX_EXPRESSION.op.type == assignmentOps[i]) {
            return true;
        }
    }

    return false;
}

int32_t getStructIndex(CometCompiler* c, CometStruct* needle) {
    for (size_t i = 0; i < c->structs.count; i++) {
        CometStruct* structType = *get(c->structs, i);

        if (structType == needle) {
            return i;
        }
    }

    return -1;
}

int32_t getMethodIndex(CometStruct* structType, char* methodName) {
    for (size_t i = 0; i < structType->numMethods; i++) {
        if (!structType->vtable[i]) {
            break;
        }

        if (strcmp(structType->vtable[i]->name, methodName) == 0) {
            return i;
        }
    }

    return -1;
}

ResultType(CometOperand, ErrorMessage) loadExternalLib(CometCompiler* c, const char* path, char* libName) {
    void* handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        char* dlErrMsg = dlerror();
        Estr buffer = CREATE_ESTR("Failed to load module: ");
        APPEND_ESTR(buffer, dlErrMsg);

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "ExternalLibError",
            buffer.str,
            NULL,
            1,
            1,
            1
        );
        return Error(CometOperand, ErrorMessage, errMsg);
    }

    dlerror();
    void (*onLibImport)(CometEnvironment* env) = dlsym(handle, "onImport");
    if (!onLibImport) {
        dlclose(handle);

        char* errorMsg = dlerror();
        Estr buffer = CREATE_ESTR("Could not get the \"onImport\" function from the library \"");
        APPEND_ESTR(buffer, libName);
        APPEND_ESTR(buffer, "\". (");
        APPEND_ESTR(buffer, errorMsg);
        APPEND_ESTR(buffer, ")");

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "ExternalLibError",
            buffer.str,
            NULL,
            1,
            1,
            1
        );
        return Error(CometOperand, ErrorMessage, errMsg);
    }

    uint8_t libIdx = c->libs.count;
    char* storedLibName = calloc(1, 64);

    // make sure we dont overrun the buffer
    size_t srcSize = strlen(libName) + 1;
    memcpy(storedLibName, libName, srcSize < 64 ? srcSize : 64);
    append(c->libs, storedLibName);

    CometEnvironment* oldEnv = c->env;
    CometEnvironment* libEnv = newEnvironment("externalLib", c->env, false); 
    onLibImport(libEnv);

    // whenever an external lib creates a function we need to create a symbol for it in the compiler

    Record* current, *tmp;

    // iterate over functions first
    HASH_ITER(hh, libEnv->records, current, tmp) {

        switch (current->type.typeKind) {
            case COMET_FUNCTION: {
                CometFunction* funcVal = (CometFunction*)current->value.imm.bigVal; // i sure do love casting pointers to ints lmao
                funcVal->libIdx = libIdx;

                // doin it manually lo-
                c->functions[c->functionCount] = funcVal;
                CometOperand funcOperand = createOperand(CO_SYMBOL);
                funcOperand.symbolIdx = c->functionCount;

                c->functionCount++;

                current->value = funcOperand;
                break;
            }

            default: {
                continue;
            }
        }

        
    }

    // then iterate over structs after we know all the funcs exist
    HASH_ITER(hh, libEnv->records, current, tmp) {
        switch (current->type.typeKind) {
            case COMET_TYPE: {
                CometStruct* structVal = current->value.imm.typeVal.structType;

                CometMethod** vtable = malloc(sizeof(CometMethod*) * structVal->numMethods);
                for (size_t i = 0; i < structVal->numMethods; i++) {
                    CometFunction* func = (CometFunction*)(structVal->vtable[i]);

                    CometMethod* method = malloc(sizeof(CometMethod));
                    method->argCount = func->argCount;
                    method->returnType = func->returnType;
                    method->blockIdx = func->blockIdx;
                    memcpy(method->name, func->name, 32);

                    // find symbol idx

                    bool found = false;
                    for (size_t funcIdx = 0; funcIdx < c->functionCount; funcIdx++) {

                        if (c->functions[funcIdx] == func) {
                            found = true;

                            method->symbolIdx = funcIdx;

                            vtable[i] = method;
                            break;
                        }
                    }

                    if (!found) {
                        Estr buffer = CREATE_ESTR("Could not find function \"");
                        APPEND_ESTR(buffer, func->name);
                        APPEND_ESTR(buffer, "\" and I don't know why.");

                        ErrorMessage errMsg = createError(
                            c->inputFilePath,
                            c->sourceCode,
                            "ExternalLibError",
                            buffer.str,
                            NULL,
                            1,
                            1,
                            1
                        );
                        return Error(CometOperand, ErrorMessage, errMsg);
                    }
                }
                structVal->vtable = vtable;

                append(c->structs, structVal);
                break;
            }

            default:
                break;
        }
        
    }

    CometOperand libValue = createOperand(CO_IMMEDIATE);
    libValue.imm.typeKind = COMET_MODULE;
    libValue.imm.moduleVal = libEnv;

    CometType libType = {
        .typeKind = COMET_MODULE
    };

    defineVar(oldEnv, libName, RECORD_LOCAL, libValue, libType, false);

    dlclose(handle);
    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}

ResultType(CometOperand, ErrorMessage) visitProgram(CometCompiler* c, CometASTNode* p) {
    for (size_t i = 0; i < p->data.AST_PROGRAM.numStatements; i++) {
        ResultType(CometOperand, ErrorMessage) result = compile(c, p->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;
    }

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}

int rankType(CometType type) {
    switch (type.typeKind) {
        case COMET_BOOL: return 1;
        case COMET_SMALL: return 2;
        case COMET_INT: return 3;
        case COMET_BIG: return 4;
        case COMET_FLOAT: return 5;
        case COMET_DOUBLE: return 6;
        case COMET_STRUCT: return 7;
        default: return 0;
    }
}

CometType unifyType(CometType a, CometType b) {
    return (rankType(a) > rankType(b)) ? a : b;
}

bool canImplicitCastType(CometType target, CometType type) {
    if (typeIsInt(target) && typeIsInt(type))
        return true;

    if (typeIsFloat(target) && typeIsFloat(type))
        return true;

    if (target.typeKind == COMET_ARRAY && type.typeKind == COMET_ARRAY) {
        if (target.arrayType->dims != type.arrayType->dims)
            return false;

        for (size_t i = 0; i < target.arrayType->dims; i++) {
            if (target.arrayType->isFixedSize[i] != type.arrayType->isFixedSize[i])
                return false;

            if (target.arrayType->fixedSize[i] != type.arrayType->fixedSize[i])
                return false;
        }

        return canImplicitCastType(*target.arrayType->elem, *type.arrayType->elem);
    }

    return false;
}

ResultType(CometOperand, ErrorMessage) visitInfixExpression(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, ErrorMessage) visitPrefixExpression(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, ErrorMessage) visitFuncCall(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, ErrorMessage) visitNewStatement(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, ErrorMessage) visitValue(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;

    switch (node->nodeType) {
        case AST_INFIX_EXPRESSION:
            return visitInfixExpression(c, node);

        case AST_PREFIX_EXPRESSION:
            return visitPrefixExpression(c, node);

        case AST_FUNC_CALL:
            return visitFuncCall(c, node);

        case AST_INT: {
            CometOperand new = createOperand(CO_IMMEDIATE);
            new.imm.typeKind = COMET_BIG;
            new.imm.bigVal = node->data.AST_INT.number;

            CometOperand idx = storeConst(c, new);
            buildPushConst(c, idx);

            return Success(CometOperand, ErrorMessage, new);
        }

        case AST_BOOL: {
            CometOperand new = createOperand(CO_IMMEDIATE);
            new.imm.typeKind = COMET_BOOL;
            new.imm.boolVal = node->data.AST_BOOL.value;

            CometOperand idx = storeConst(c, new);
            buildPushConst(c, idx);

            return Success(CometOperand, ErrorMessage, new);
        }

        case AST_DOUBLE: {
            CometOperand new = createOperand(CO_IMMEDIATE);
            new.imm.typeKind = COMET_DOUBLE;
            new.imm.doubleVal = node->data.AST_DOUBLE.number;

            CometOperand idx = storeConst(c, new);
            buildPushConst(c, idx);

            return Success(CometOperand, ErrorMessage, new);
        }

        case AST_STRING: {
            for (size_t i = 0; i < strlen(node->data.AST_STRING.value); i++) {
                CometOperand charVal = createOperand(CO_IMMEDIATE);
                charVal.imm.typeKind = COMET_SMALL;
                charVal.imm.smallVal = node->data.AST_STRING.value[i];

                CometOperand charIdx = storeConst(c, charVal);
                buildPushConst(c, charIdx);
            }

            // push zero
            CometOperand zero = createOperand(CO_IMMEDIATE);
            zero.imm.typeKind = COMET_SMALL;
            zero.imm.intVal = 0;

            CometOperand zeroIdx = storeConst(c, zero);
            buildPushConst(c, zeroIdx);

            // push size
            CometOperand sizeVal = createOperand(CO_IMMEDIATE);
            sizeVal.imm.typeKind = COMET_INT;
            sizeVal.imm.intVal = strlen(node->data.AST_STRING.value) + 1;

            CometOperand sizeIdx = storeConst(c, sizeVal);
            buildPushConst(c, sizeIdx);

            CometOperand new = buildBuildList(c);

            return Success(CometOperand, ErrorMessage, new);
        }

        case AST_ARRAY: {
            for (size_t i = 0; i < node->data.AST_ARRAY.elements.count; i++) {
                ResultType(CometOperand, ErrorMessage) elementValue = visitValue(c, *get(node->data.AST_ARRAY.elements, i));
                if (elementValue.error)
                    return elementValue;
            }

            // push size
            CometOperand sizeVal = createOperand(CO_IMMEDIATE);
            sizeVal.imm.typeKind = COMET_INT;
            sizeVal.imm.intVal = (int32_t)node->data.AST_ARRAY.elements.count;

            CometOperand idx = storeConst(c, sizeVal);
            buildPushConst(c, idx);

            CometOperand new = buildBuildList(c);

            return Success(CometOperand, ErrorMessage, new);
        }

        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(c->env, varName);

            if (varRecord == NULL) {
                Estr buffer = CREATE_ESTR("Undefined variable \"");
                APPEND_ESTR(buffer, varName);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "UndefinedVariable",
                    buffer.str,
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );
                return Error(CometOperand, ErrorMessage, errMsg);
            }

            uint32_t idx = varRecord->recordIdx;

            CometOperand new;
            switch (varRecord->recordType) {
                case RECORD_LOCAL: 
                   new = buildLoad(c, idx);
                   break;
                case RECORD_ARG:
                    new = buildLoadArg(c, idx);
                    break;
                
            }
             
            return Success(CometOperand, ErrorMessage, new);
            
        }

        case AST_NEW_STATEMENT: 
            return visitNewStatement(c, node);

        default: {
            Estr buffer = CREATE_ESTR("Could not build expression: \"");
            APPEND_ESTR(buffer, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(buffer, "\"");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "CompilerIssue",
                buffer.str,
                NULL,
                node->lineNum,
                node->startCol,
                node->endCol
            );
            return Error(CometOperand, ErrorMessage, errMsg);
        }
    }
}

ResultType(CometOperand, ErrorMessage) getModuleValue(CometCompiler* c, CometASTNode* infixExpr);
ResultType(CometType, ErrorMessage) resolveType(CometCompiler* c, CometASTNode* node);
ResultType(CometFunctionTypeInfo, ErrorMessage) getFunction(CometCompiler* c, CometASTNode* node, bool buildValues) {
    switch (node->nodeType) {
        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(c->env, varName);

            if (varRecord == NULL) {
                Estr buffer = CREATE_ESTR("Undefined function \"");
                APPEND_ESTR(buffer, varName);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "UndefinedFunction",
                    buffer.str,
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );
                return Error(CometFunctionTypeInfo, ErrorMessage, errMsg);
            }

            if (varRecord->type.typeKind != COMET_FUNCTION) {
                break;
            }

            CometFunctionTypeInfo functionInfo = {
                .funcType = FUNC_FUNC,
                .value = varRecord->value,
                .methodIdx = (CometOperand){
                    .type = CO_NONE
                }
            };

            return Success(CometFunctionTypeInfo, ErrorMessage, functionInfo);
        }

        case AST_INFIX_EXPRESSION: {
            struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

            char* fieldName = expr.right->data.AST_IDENTIFIER.ident;

            ResultType(CometType, ErrorMessage) structType = resolveType(c, expr.left);
            if (structType.error)
                return Error(CometFunctionTypeInfo, ErrorMessage, structType.as.error);

            // get function from module
            if (structType.as.success.typeKind == COMET_MODULE) {
                ResultType(CometOperand, ErrorMessage) value = getModuleValue(c, node);
                if (value.error)
                    return Error(CometFunctionTypeInfo, ErrorMessage, value.as.error);

                if (value.as.success.type != CO_SYMBOL) {
                    Estr buffer = CREATE_ESTR("Attribute \"");
                    APPEND_ESTR(buffer, fieldName);
                    APPEND_ESTR(buffer, "\" is not a function");

                    ErrorMessage errMsg = createError(
                        c->inputFilePath,
                        c->sourceCode,
                        "NotAFunction",
                        buffer.str,
                        NULL,
                        node->lineNum,
                        node->startCol,
                        node->endCol
                    );
                    return Error(CometFunctionTypeInfo, ErrorMessage, errMsg);
                }

                CometFunctionTypeInfo functionInfo = {
                    .funcType = FUNC_FUNC,
                    .value = value.as.success,
                    .methodIdx = (CometOperand){
                        .type = CO_NONE
                    }
                };
                return Success(CometFunctionTypeInfo, ErrorMessage, functionInfo);
            }

            if (buildValues) {
                ResultType(CometOperand, ErrorMessage) structValue = visitValue(c, expr.left);
                if (structValue.error)
                    return Error(CometFunctionTypeInfo, ErrorMessage, structValue.as.error);
            }

            

            int32_t methodIdx = getMethodIndex(structType.as.success.structType, fieldName);
            if (methodIdx == -1) {
                Estr buffer = CREATE_ESTR("There is no method called \"");
                APPEND_ESTR(buffer, fieldName);
                APPEND_ESTR(buffer, "\" in the struct \"");
                APPEND_ESTR(buffer, structType.as.success.structType->name);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "MethodNotFound",
                    buffer.str,
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );
                return Error(CometFunctionTypeInfo, ErrorMessage, errMsg);
            }

            CometOperand funcValue = createOperand(CO_SYMBOL);
            funcValue.symbolIdx = structType.as.success.structType->vtable[methodIdx]->symbolIdx;

            CometOperand methodIdxValue = createOperand(CO_IMMEDIATE);
            methodIdxValue.imm.typeKind = COMET_SMALL;
            methodIdxValue.imm.smallVal = methodIdx;


            CometFunctionTypeInfo methodInfo = {
                .funcType = FUNC_METHOD,
                .value = funcValue,
                .methodIdx = methodIdxValue
            };

            return Success(CometFunctionTypeInfo, ErrorMessage, methodInfo);
        }

        default: break;
    }

    return Error(CometFunctionTypeInfo, ErrorMessage, "Attempted to call something which isn't a function!");
}

ResultType(CometType, ErrorMessage) getTypeByName(CometCompiler* c, char* typeName, CometASTNode* node) {

    CometTypeMapEntry* entry = lookupType(c->typeMap, typeName);
    if (entry)
        return Success(CometType, ErrorMessage, entry->type);

    Estr buffer = CREATE_ESTR("Cannot find type with name \"");
    APPEND_ESTR(buffer, typeName);
    APPEND_ESTR(buffer, "\"");

    ErrorMessage errMsg = createError(
        c->inputFilePath,
        c->sourceCode,
        "UnkownType",
        buffer.str,
        NULL,
        node->lineNum,
        node->startCol,
        node->endCol
    );
    return Error(CometType, ErrorMessage, errMsg);

}

CometType flattenArrayType(CometType type) {
    CometArrayType* outArrayType = calloc(1, sizeof(CometArrayType));
    if (!outArrayType) 
        return (CometType){ .typeKind = COMET_VOID };
    
    CometType out = {
        .typeKind = COMET_ARRAY,
        .arrayType = outArrayType
    };

    if (type.arrayType->elem->typeKind != COMET_ARRAY) {
        CometType* elem = malloc(sizeof(CometType));
        if (!elem)
            return (CometType){ .typeKind = COMET_VOID };

        *elem = type;

        outArrayType->elem = elem;
        outArrayType->dims = 1;
        return out;
    }

    

    CometArrayType* innerArray = flattenArrayType(*type.arrayType->elem).arrayType;

    for (size_t i = 0; i < MAX_ARRAY_DEPTH; i++) {
        if (type.arrayType->isFixedSize[i]) {
            outArrayType->isFixedSize[i] = type.arrayType->isFixedSize[i];
            outArrayType->fixedSize[i] = type.arrayType->fixedSize[i];
        } else {
            outArrayType->isFixedSize[i] = innerArray->isFixedSize[i];
            outArrayType->fixedSize[i] = innerArray->fixedSize[i];
        }
    }

    outArrayType->elem = innerArray->elem->arrayType->elem;
    outArrayType->dims = type.arrayType->dims + innerArray->dims; // NOTE: I HAVE NO CLUE IF THIS WORKS OR NOT LMAO

    return out;
}

ResultType(cometTypePtr, ErrorMessage) visitStructDefStatement(CometCompiler* c, CometASTNode* node, bool isGenericInstantiation, char* genericNameEnding);
ResultType(CometType, ErrorMessage) getType(CometCompiler* c, CometASTNode* typeNode);
ResultType(cometTypePtr, ErrorMessage) generateGenericStruct(CometCompiler* c, GenericStructDef structDef, char* structName, List(astNodePtr) genericTypes) {
    // check if it already exists
    for (size_t structIdx = 0; structIdx < c->cachedGenerics.count; structIdx++) {
        CachedGenericStruct cachedGeneric = *get(c->cachedGenerics, structIdx);

        if (cachedGeneric.structDef.name == structDef.name && cachedGeneric.genericTypes.count == genericTypes.count) {
            bool found = true;
            for (size_t typeIdx = 0; typeIdx < cachedGeneric.genericTypes.count; typeIdx++) {
                ResultType(CometType, ErrorMessage) givenType = getType(c, *get(genericTypes, typeIdx));
                if (givenType.error)
                    return Error(cometTypePtr, ErrorMessage, givenType.as.error);

                if (!typesAreEqual(
                    *get(cachedGeneric.genericTypes, typeIdx),
                    givenType.as.success
                )) {
                    found = false;
                    break;
                }
            }
            if (!found)
                continue;

            CometType* type = malloc(sizeof(CometType));
            type->typeKind = COMET_STRUCT;
            type->structType = cachedGeneric.structType;

            return Success(cometTypePtr, ErrorMessage, type);
        }
    }

    // resolve generic type names
    c->typeMap = newTypemap(c->typeMap);

    nodeList genericTypeNames = structDef.structDefNode->data.AST_STRUCT_DEF_STATEMENT.genericTypes;
    List(CometType) resolvedGenericTypes = newList(CometType);

    for (size_t i = 0; i < genericTypeNames.count; i++) {
        CometASTNode* genericTypeNode = *get(genericTypeNames, i);
        char* genericTypeName = genericTypeNode->data.AST_IDENTIFIER.ident;

        ResultType(CometType, ErrorMessage) resolvedType = getType(c, *get(genericTypes, i));
        if (resolvedType.error)
            return Error(cometTypePtr, ErrorMessage, resolvedType.as.error);

        bool success = defineType(c->typeMap, genericTypeName, resolvedType.as.success);
        if (!success) {
            Estr buffer = CREATE_ESTR("Duplicate generic type name \"");
            APPEND_ESTR(buffer, genericTypeName);
            APPEND_ESTR(buffer, "\"");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "DuplicateTypedef",
                buffer.str,
                NULL,
                genericTypeNode->lineNum,
                genericTypeNode->startCol,
                genericTypeNode->endCol
            );

            return Error(cometTypePtr, ErrorMessage, errMsg);
        }

        append(resolvedGenericTypes, resolvedType.as.success);
    }

    Estr genericNameEnding = CREATE_ESTR("");
    for (size_t i = 0; i < resolvedGenericTypes.count; i++) {
        APPEND_ESTR(genericNameEnding, "_");
        APPEND_ESTR(genericNameEnding, typeToString(*get(resolvedGenericTypes, i)));
    }

    ResultType(cometTypePtr, ErrorMessage) genericStructInstance = visitStructDefStatement(c, structDef.structDefNode, true, genericNameEnding.str);
    if (genericStructInstance.error)
        return Error(cometTypePtr, ErrorMessage, genericStructInstance.as.error);

    

    c->typeMap = destroyTypeMap(c->typeMap);

    genericStructInstance.as.success->structType->givenGenericTypes = resolvedGenericTypes.pointer;
    genericStructInstance.as.success->structType->numGivenGenericTypes = resolvedGenericTypes.count;

    // cache the new generic
    for (size_t structIdx = 0; structIdx < c->genericDefinitions.count; structIdx++) {
        GenericStructDef cometStruct = *get(c->genericDefinitions, structIdx);

        if (strcmp(cometStruct.name, structName) == 0) {
            CachedGenericStruct cached = {
                .structType = genericStructInstance.as.success->structType,
                .genericTypes = resolvedGenericTypes,
                .baseStructName = cometStruct.name,
                .structDef = structDef
            };

            append(c->cachedGenerics, cached);
            break;
        }   
    }
    

    return Success(cometTypePtr, ErrorMessage, genericStructInstance.as.success);
}

ResultType(cometTypePtr, ErrorMessage) findBuiltinType(CometCompiler* c, CometASTNode* node, nodeList genericTypes) {
    char* baseTypeName = node->data.AST_IDENTIFIER.ident;

    CometTypeMapEntry* entry = lookupType(c->typeMap, baseTypeName);
    if (entry) {
        return Success(cometTypePtr, ErrorMessage, &entry->type);
    }

    // maybe its a generic struct?
    for (size_t genericIdx = 0; genericIdx < c->genericDefinitions.count; genericIdx++) {
        GenericStructDef structDef = *get(c->genericDefinitions, genericIdx);

        if (strcmp(structDef.name, baseTypeName) == 0) {
            return generateGenericStruct(c, structDef, baseTypeName, genericTypes);
        }
    }

    Estr buffer = CREATE_ESTR("Unkown type \"");
    APPEND_ESTR(buffer, baseTypeName);
    APPEND_ESTR(buffer, "\"");

    ErrorMessage errMsg = createError(
        c->inputFilePath,
        c->sourceCode,
        "UnkownType",
        buffer.str,
        NULL,
        node->lineNum,
        node->startCol,
        node->endCol
    );

    return Error(cometTypePtr, ErrorMessage, errMsg);
}

ResultType(cometTypePtr, ErrorMessage) getBaseType(CometCompiler* c, nodeList chain, nodeList genericTypes) {

    ResultType(cometTypePtr, ErrorMessage) builtinType = findBuiltinType(c, *get(chain, 0), genericTypes);
    if (!builtinType.error)
        return builtinType;

    for (size_t i = 0; i < chain.count; i++) {
        CometASTNode* currentNode = *get(chain, 0);

        ResultType(CometType, ErrorMessage) currentType = resolveType(c, currentNode);
        if (currentType.error)
            return Error(cometTypePtr, ErrorMessage, currentType.as.error);

        CometType current = currentType.as.success;
        char* currentName = currentNode->data.AST_IDENTIFIER.ident;

        switch (current.typeKind) {
            case COMET_MODULE: {
                Record* moduleRecord = lookup(c->env, currentName);
                if (!moduleRecord) {
                    Estr buffer = CREATE_ESTR("Undefined module \"");
                    APPEND_ESTR(buffer, currentName);
                    APPEND_ESTR(buffer, "\"");

                    ErrorMessage errMsg = createError(
                        c->inputFilePath,
                        c->sourceCode,
                        "UndefinedModule",
                        buffer.str,
                        NULL,
                        currentNode->lineNum,
                        currentNode->startCol,
                        currentNode->endCol
                    );

                    return Error(cometTypePtr, ErrorMessage, errMsg);
                }

                CometOperand module = moduleRecord->value;

                if (module.imm.typeKind != COMET_MODULE) {
                    ErrorMessage errMsg = createError(
                        c->inputFilePath,
                        c->sourceCode,
                        "TypeError",
                        "Attempted to get an attribute from something that isn't a module!",
                        NULL,
                        currentNode->lineNum,
                        currentNode->startCol,
                        currentNode->endCol
                    );

                    return Error(cometTypePtr, ErrorMessage, errMsg);
                }

                if (i == chain.count - 1) {
                    Estr buffer = CREATE_ESTR("Module \"");
                    APPEND_ESTR(buffer, currentName);
                    APPEND_ESTR(buffer, "\" is not a type.");

                    ErrorMessage errMsg = createError(
                        c->inputFilePath,
                        c->sourceCode,
                        "NotAType",
                        buffer.str,
                        NULL,
                        currentNode->lineNum,
                        currentNode->startCol,
                        currentNode->endCol
                    );

                    return Error(cometTypePtr, ErrorMessage, errMsg);
                }

                CometASTNode* next = *get(chain, i+1);
                char* attribName = next->data.AST_IDENTIFIER.ident;

                Record* attribRecord = lookup(module.imm.moduleVal, attribName);
                if (!attribRecord) {
                    Estr buffer = CREATE_ESTR("Can't find attribute \"");
                    APPEND_ESTR(buffer, attribName);
                    APPEND_ESTR(buffer, "\" in module \"");
                    APPEND_ESTR(buffer, currentName);
                    APPEND_ESTR(buffer, "\"");

                    ErrorMessage errMsg = createError(
                        c->inputFilePath,
                        c->sourceCode,
                        "UnkownAttribute",
                        buffer.str,
                        NULL,
                        next->lineNum,
                        next->startCol,
                        next->endCol
                    );

                    return Error(cometTypePtr, ErrorMessage, errMsg);
                }

                if (attribRecord->type.typeKind != COMET_TYPE) {
                    Estr buffer = CREATE_ESTR("Attribute \"");
                    APPEND_ESTR(buffer, attribName);
                    APPEND_ESTR(buffer, "\" in module \"");
                    APPEND_ESTR(buffer, currentName);
                    APPEND_ESTR(buffer, "\" is not a type");

                    char* typeStr = typeToString(attribRecord->type);

                    Estr help = CREATE_ESTR("It is of type ");
                    APPEND_ESTR(help, typeStr);

                    ErrorMessage errMsg = createError(
                        c->inputFilePath,
                        c->sourceCode,
                        "NotAType",
                        buffer.str,
                        help.str,
                        next->lineNum,
                        next->startCol,
                        next->endCol
                    );

                    return Error(cometTypePtr, ErrorMessage, errMsg);
                }

                CometType type = attribRecord->value.imm.typeVal;

                if (genericTypes.count > 0) {
                    // map given types to struct generic types
                    List(GenericTypeMapping) mapping = newList(GenericTypeMapping);

                    CometStruct* structType = type.structType;
                    for (uint8_t i = 0; i < structType->numGenericTypes; i++) {
                        ResultType(CometType, ErrorMessage) currentType = getType(c, *get(genericTypes, i));
                        if (currentType.error)
                            return Error(cometTypePtr, ErrorMessage, currentType.as.error);

                        GenericTypeMapping genericNameToType = (GenericTypeMapping){
                            .genericTypeName = structType->genericTypeNames[i],
                            .newType = currentType.as.success
                        };

                        append(mapping, genericNameToType);
                    }

                    CometStruct* genericStructWithTypes = getGenericStruct(c, structType, mapping);
            
                    CometType* typePtr = malloc(sizeof(CometType));
                    typePtr->typeKind = COMET_STRUCT;
                    typePtr->structType = genericStructWithTypes;

                    return Success(cometTypePtr, ErrorMessage, typePtr);
                }

                return Success(cometTypePtr, ErrorMessage, &attribRecord->value.imm.typeVal);
            }

            default: {
                Estr buffer = CREATE_ESTR("Cannot get type from \"");
                APPEND_ESTR(buffer, currentName);
                APPEND_ESTR(buffer, "\" because it's not a module.");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "TypeMismatch",
                    buffer.str,
                    NULL,
                    currentNode->lineNum,
                    currentNode->startCol,
                    currentNode->endCol
                );

                return Error(cometTypePtr, ErrorMessage, errMsg);
            }
        }
    }

    ErrorMessage errMsg = createError(
        c->inputFilePath,
        c->sourceCode,
        "CompilerIssue",
        "This error should never happen, please report this as a bug! (getBaseType)",
        NULL,
        chain.pointer[0]->lineNum,
        chain.pointer[0]->startCol,
        chain.pointer[chain.count]->endCol
    );

    return Error(cometTypePtr, ErrorMessage, errMsg);
}

ResultType(CometType, ErrorMessage) getType(CometCompiler* c, CometASTNode* typeNode) {
    struct AST_TYPE type = typeNode->data.AST_TYPE;

    // get base type
    ResultType(cometTypePtr, ErrorMessage) baseType = getBaseType(c, type.baseType, typeNode->data.AST_TYPE.genericTypes);
    if (baseType.error)
        return Error(CometType, ErrorMessage, baseType.as.error);

    CometType finalType;    
    if (type.dimensions > 0) {
        finalType.typeKind = COMET_ARRAY;

        CometArrayType* arrayType = malloc(sizeof(CometArrayType));
        arrayType->elem = baseType.as.success;

        if (type.shape.count > MAX_ARRAY_DEPTH) {
            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "MaxArrayDepthExceeded",
                "how deep is that array you're making?????",
                NULL,
                typeNode->lineNum,
                typeNode->startCol,
                typeNode->endCol
            );

            return Error(CometType, ErrorMessage, errMsg);
        }

        arrayType->dims = type.dimensions;

        for (size_t i = 0; i < type.dimensions; i++) {
            if (i >= type.shape.count) {
                arrayType->isFixedSize[i] = false;
                continue;
            }

            CometASTNode* shapeNode = *get(type.shape, i);

            if (shapeNode->nodeType == AST_INT) {
                arrayType->isFixedSize[i] = true;
                arrayType->fixedSize[i] = shapeNode->data.AST_INT.number;
            } else {
                arrayType->isFixedSize[i] = false;
            }
        }

        finalType.arrayType = arrayType;
    } else {
        finalType = *baseType.as.success;
    }

    if (finalType.typeKind == COMET_ARRAY && finalType.arrayType->elem->typeKind == COMET_ARRAY) {
        finalType = flattenArrayType(finalType);
    }

    return Success(CometType, ErrorMessage, finalType);
}

ResultType(voidPtr, ErrorMessage) visitLValue(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;

    switch (node->nodeType) {
        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(c->env, varName);

            if (varRecord == NULL) {
                Estr buffer = CREATE_ESTR("Undefined variable \"");
                APPEND_ESTR(buffer, varName);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "UndefinedVariable",
                    buffer.str,
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );

                return Error(voidPtr, ErrorMessage, errMsg);
            }

            uint32_t idx = varRecord->recordIdx;

            switch (varRecord->recordType) {
                case RECORD_LOCAL: 
                   buildLoad(c, idx);
                   break;
                case RECORD_ARG:
                    buildLoadArg(c, idx);
                    break;
                
            }
  
            return Success(voidPtr, ErrorMessage, NULL);
        }

        case AST_INFIX_EXPRESSION: {
            struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

            ResultType(CometType, ErrorMessage) leftType = resolveType(c, expr.left);
            if (leftType.error)
                return Error(voidPtr, ErrorMessage, leftType.as.error);

            ResultType(CometOperand, ErrorMessage) left = visitValue(c, expr.left);
            if (left.error)
                return Error(voidPtr, ErrorMessage, left.as.error);


            switch (expr.op.type) {
                case CT_DOT: {
                    if (leftType.as.success.typeKind != COMET_STRUCT) {
                        ErrorMessage errMsg = createError(
                            c->inputFilePath,
                            c->sourceCode,
                            "TypeError",
                            "Attempted to set field of something that isn't a struct!",
                            NULL,
                            node->lineNum,
                            node->startCol,
                            node->endCol
                        );

                        return Error(voidPtr, ErrorMessage, errMsg);
                    }

                    char* fieldName = expr.right->data.AST_IDENTIFIER.ident;
                    uint32_t fieldIndex = getFieldIndex(leftType.as.success.structType, fieldName);
                    if (fieldIndex == -1) {
                        Estr buffer = CREATE_ESTR("Struct \"");
                        APPEND_ESTR(buffer, typeToString(leftType.as.success));
                        APPEND_ESTR(buffer, "\" doesn't have a field named \"");
                        APPEND_ESTR(buffer, fieldName);
                        APPEND_ESTR(buffer, "\"");

                        ErrorMessage errMsg = createError(
                            c->inputFilePath,
                            c->sourceCode,
                            "FieldNotFound",
                            buffer.str,
                            NULL,
                            node->lineNum,
                            node->startCol,
                            node->endCol
                        );

                        return Error(voidPtr, ErrorMessage, errMsg);
                    }

                    buildGetField(c, fieldIndex);
                    break;
                }

                case CT_COLON: {
                    if (leftType.as.success.typeKind != COMET_ARRAY) {
                        ErrorMessage errMsg = createError(
                            c->inputFilePath,
                            c->sourceCode,
                            "TypeError",
                            "Attempted to set element of something that isn't an array!",
                            NULL,
                            node->lineNum,
                            node->startCol,
                            node->endCol
                        );

                        return Error(voidPtr, ErrorMessage, errMsg);
                    }

                    ResultType(CometOperand, ErrorMessage) index = visitValue(c, expr.right);
                    if (index.error)
                        return Error(voidPtr, ErrorMessage, index.as.error);
                    
                    break;
                }

                default: {
                    Estr buffer = CREATE_ESTR("Cannot use operator \"");
                    APPEND_ESTR(buffer, tokenTypeToCStr(expr.op.type));
                    APPEND_ESTR(buffer, "\" in lvalue.");

                    ErrorMessage errMsg = createError(
                        c->inputFilePath,
                        c->sourceCode,
                        "InvalidOperator",
                        buffer.str,
                        NULL,
                        node->lineNum,
                        node->startCol,
                        node->endCol
                    );

                    return Error(voidPtr, ErrorMessage, errMsg);
                }
            }

            break;
        }

        default: {
            Estr buffer = CREATE_ESTR("\"");
            APPEND_ESTR(buffer, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(buffer, "\" cannot be an lvalue.");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "InvalidOperator",
                buffer.str,
                NULL,
                node->lineNum,
                node->startCol,
                node->endCol
            );

            return Error(voidPtr, ErrorMessage, errMsg);
        }
    }

    return Success(voidPtr, ErrorMessage, NULL);
}

ResultType(CometOperand, ErrorMessage) getModuleValue(CometCompiler* c, CometASTNode* infixExpr) {
    struct AST_INFIX_EXPRESSION expr = infixExpr->data.AST_INFIX_EXPRESSION;

    switch (expr.left->nodeType) {
        case AST_IDENTIFIER: {
            char* moduleName = expr.left->data.AST_IDENTIFIER.ident;
            Record* moduleRecord = lookup(c->env, moduleName);
            if (!moduleRecord) {
                Estr buffer = CREATE_ESTR("Undefined module \"");
                APPEND_ESTR(buffer, moduleName);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "UndefinedModule",
                    buffer.str,
                    NULL,
                    infixExpr->lineNum,
                    infixExpr->startCol,
                    infixExpr->endCol
                );

                return Error(CometOperand, ErrorMessage, errMsg);
            }

            CometOperand module = moduleRecord->value;

            if (module.imm.typeKind != COMET_MODULE) {
                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "TypeError",
                    "Attempted to get an attribute from something that isn't a module!",
                    NULL,
                    infixExpr->lineNum,
                    infixExpr->startCol,
                    infixExpr->endCol
                );

                return Error(CometOperand, ErrorMessage, errMsg);
            }

            char* attribName = expr.right->data.AST_IDENTIFIER.ident;

            Record* attribRecord = lookup(module.imm.moduleVal, attribName);
            if (!attribRecord) {
                Estr buffer = CREATE_ESTR("Can't find attribute \"");
                APPEND_ESTR(buffer, attribName);
                APPEND_ESTR(buffer, "\" in module \"");
                APPEND_ESTR(buffer, expr.left->data.AST_IDENTIFIER.ident);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "UnkownAttribute",
                    buffer.str,
                    NULL,
                    infixExpr->lineNum,
                    infixExpr->startCol,
                    infixExpr->endCol
                );

                return Error(CometOperand, ErrorMessage, errMsg);
            }

            return Success(CometOperand, ErrorMessage, attribRecord->value);
        }

        case AST_INFIX_EXPRESSION: {
            char* moduleName = expr.left->data.AST_IDENTIFIER.ident;
            Record* moduleRecord = lookup(c->env, moduleName);
            if (!moduleRecord) {
                Estr buffer = CREATE_ESTR("Undefined module \"");
                APPEND_ESTR(buffer, moduleName);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "UndefinedModule",
                    buffer.str,
                    NULL,
                    infixExpr->lineNum,
                    infixExpr->startCol,
                    infixExpr->endCol
                );

                return Error(CometOperand, ErrorMessage, errMsg);
            }
            CometOperand module = moduleRecord->value;

            char* attribName = expr.right->data.AST_IDENTIFIER.ident;

            Record* attribRecord = lookup(module.imm.moduleVal, attribName);
            if (!attribRecord) {
                Estr buffer = CREATE_ESTR("Can't find attribute \"");
                APPEND_ESTR(buffer, attribName);
                APPEND_ESTR(buffer, "\" in module \"");
                APPEND_ESTR(buffer, expr.left->data.AST_IDENTIFIER.ident);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "UnkownAttribute",
                    buffer.str,
                    NULL,
                    infixExpr->lineNum,
                    infixExpr->startCol,
                    infixExpr->endCol
                );

                return Error(CometOperand, ErrorMessage, errMsg);
            }

            return Success(CometOperand, ErrorMessage, attribRecord->value);
        }

        default: {
            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "Bug",
                "getModuleValue: this error should never happen, please make a bug report.",
                NULL,
                infixExpr->lineNum,
                infixExpr->startCol,
                infixExpr->endCol
            );

            return Error(CometOperand, ErrorMessage, errMsg);
        }
    }
}

ResultType(CometType, ErrorMessage) getModuleAttribType(CometCompiler* c, CometASTNode* node) {
    struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

    if (expr.right->nodeType != AST_IDENTIFIER) {
        Estr buffer = CREATE_ESTR("Expected attribute name after \".\" but got \"");
        APPEND_ESTR(buffer, ASTNodeTypeToCStr(expr.right->nodeType));
        APPEND_ESTR(buffer, "\"");

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "InvalidSyntax",
            buffer.str,
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometType, ErrorMessage, errMsg);
    }

    switch (expr.left->nodeType) {
        case AST_IDENTIFIER: {
            char* moduleName = expr.left->data.AST_IDENTIFIER.ident;
            Record* moduleRecord = lookup(c->env, moduleName);
            if (!moduleRecord) {
                Estr buffer = CREATE_ESTR("Undefined module \"");
                APPEND_ESTR(buffer, moduleName);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "UndefinedModule",
                    buffer.str,
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );

                return Error(CometType, ErrorMessage, errMsg);
            }
            CometOperand module = moduleRecord->value;

            if (module.imm.typeKind != COMET_MODULE) {
                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "InvalidOperation",
                    "Attempted to get an attribute from something that isn't a module!",
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );

                return Error(CometType, ErrorMessage, errMsg);
            }

            char* attribName = expr.right->data.AST_IDENTIFIER.ident;

            Record* attribRecord = lookup(module.imm.moduleVal, attribName);
            if (!attribRecord) {
                Estr buffer = CREATE_ESTR("Can't find attribute \"");
                APPEND_ESTR(buffer, attribName);
                APPEND_ESTR(buffer, "\" in module \"");
                APPEND_ESTR(buffer, expr.left->data.AST_IDENTIFIER.ident);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "UndefinedAttribute",
                    buffer.str,
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );

                return Error(CometType, ErrorMessage, errMsg);
            }

            return Success(CometType, ErrorMessage, attribRecord->type);
        }

        case AST_INFIX_EXPRESSION: {
            ResultType(CometOperand, ErrorMessage) left = getModuleValue(c, expr.left);
            if (left.error)
                return Error(CometType, ErrorMessage, left.as.error);
            
            char* attribName = expr.right->data.AST_IDENTIFIER.ident;

            Record* attribRecord = lookup(left.as.success.imm.moduleVal, attribName);
            if (!attribRecord) {
                Estr buffer = CREATE_ESTR("Can't find attribute \"");
                APPEND_ESTR(buffer, attribName);
                APPEND_ESTR(buffer, "\" in module \"");
                APPEND_ESTR(buffer, expr.left->data.AST_IDENTIFIER.ident);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "UnkownAttribute",
                    buffer.str,
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );

                return Error(CometType, ErrorMessage, errMsg);
            }

            return Success(CometType, ErrorMessage, attribRecord->type);
        }

        
        default: break;
    }

    ErrorMessage errMsg = createError(
        c->inputFilePath,
        c->sourceCode,
        "InvalidSyntax",
        "Expected identifier or attribute access.",
        NULL,
        node->lineNum,
        node->startCol,
        node->endCol
    );

    return Error(CometType, ErrorMessage, errMsg);
}

CometType getTopArrayElemType(CometArrayType* arrayType) {
    

    if (arrayType->dims == 1) {
        return *arrayType->elem;
    }

    CometArrayType* outArrayType = calloc(1, sizeof(CometArrayType));
    outArrayType->elem = arrayType->elem;

    CometType outType = {
        .typeKind = COMET_ARRAY,
        .arrayType = outArrayType
    };

    outArrayType->dims = arrayType->dims - 1;
    for (size_t i = 0; i < arrayType->dims - 1; i++) {
        outArrayType->fixedSize[i] = arrayType->fixedSize[i];
        outArrayType->isFixedSize[i] = arrayType->isFixedSize[i];
    }

    return outType;
}

ResultType(CometType, ErrorMessage) resolveType(CometCompiler* c, CometASTNode* node) {
    CometValueTypeKind outTypeKind;

    switch (node->nodeType) {
        case AST_INT: outTypeKind = COMET_INT; break;
        case AST_BOOL: outTypeKind = COMET_BOOL; break;
        case AST_DOUBLE: outTypeKind = COMET_DOUBLE; break;

        case AST_STRING: {
            return getTypeByName(c, "string", node);
        }

        case AST_ARRAY: {
            List(astNodePtr) elements = node->data.AST_ARRAY.elements;

            if (elements.count < 1) {
                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "EmptyArrayInitializer",
                    "Empty array initializer",
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );

                return Error(CometType, ErrorMessage, errMsg);
            }

            CometArrayType* arrayType = malloc(sizeof(CometArrayType));
            // Clear the memory to prevent garbage data in unused depth slots
            memset(arrayType, 0, sizeof(CometArrayType)); 

            // 1. Resolve the very first element to anchor our type layout
            CometASTNode* firstElem = *get(elements, 0);
            ResultType(CometType, ErrorMessage) firstResolved = resolveType(c, firstElem);
            if (firstResolved.error) return firstResolved;

            CometType firstType = firstResolved.as.success;

            // 2. Set up the current outermost dimension (Level 0)
            arrayType->isFixedSize[0] = true;
            arrayType->fixedSize[0] = elements.count;

            // 3. Propagate deeper dimensions if the child is already an array
            if (firstType.typeKind == COMET_ARRAY) {
                arrayType->dims = firstType.arrayType->dims + 1;

                // Copy the child's dimensions, shifting them down by 1 level
                for (size_t d = 0; d < firstType.arrayType->dims; d++) {
                    arrayType->isFixedSize[d + 1] = firstType.arrayType->isFixedSize[d];
                    arrayType->fixedSize[d + 1] = firstType.arrayType->fixedSize[d];
                }

                // The immediately nested element type is the child's element type
                arrayType->elem = firstType.arrayType->elem;
            } else {
                arrayType->dims = 1;

                // It's a flat scalar array (e.g., [1, 2, 3])
                CometType* scalarType = malloc(sizeof(CometType));
                *scalarType = firstType;
                arrayType->elem = scalarType;

                for (size_t i = 1; i < MAX_ARRAY_DEPTH; i++) {
                    arrayType->isFixedSize[i] = false;
                }
            }

            // 4. Validate that all *other* elements in this literal match the first one
            for (size_t i = 1; i < elements.count; i++) {
                CometASTNode* siblingElem = *get(elements, i);
                ResultType(CometType, ErrorMessage) siblingResolved = resolveType(c, siblingElem);
                if (siblingResolved.error) return siblingResolved;

                if (!typesAreEqual(siblingResolved.as.success, firstType)) {
                    Estr helpMsg = CREATE_ESTR("You're trying to mix the types ");
                    APPEND_ESTR(helpMsg, typeToString(firstType))
                    APPEND_ESTR(helpMsg, " and ")
                    APPEND_ESTR(helpMsg, typeToString(siblingResolved.as.success))

                    // Free allocated memory here if needed to avoid leaks!
                    ErrorMessage errMsg = createError(
                        c->inputFilePath,
                        c->sourceCode,
                        "TypeError",
                        "Inconsistent element types in array literal",
                        helpMsg.str,
                        node->lineNum,
                        node->startCol,
                        node->endCol
                    );

                    return Error(CometType, ErrorMessage, errMsg);
                }
            }

            CometType outType = {
                .typeKind = COMET_ARRAY,
                .arrayType = arrayType,
            };

            return Success(CometType, ErrorMessage, outType);
        }


        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(c->env, varName);

            if (!varRecord) {
                Estr buffer = CREATE_ESTR("Undefined variable \"");
                APPEND_ESTR(buffer, varName);
                APPEND_ESTR(buffer, "\"");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "UndefinedVariable",
                    buffer.str,
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );

                return Error(CometType, ErrorMessage, errMsg);
            }

            return Success(CometType, ErrorMessage, varRecord->type);
        }
        case AST_NEW_STATEMENT: {
            ResultType(CometType, ErrorMessage) type = getType(c, node->data.AST_NEW_STATEMENT.structName);
            if (type.error)
                return type;

            CometType structType = (CometType){
                .typeKind = COMET_STRUCT,
                .structType = type.as.success.structType
            };

            return Success(CometType, ErrorMessage, structType);
        }

        case AST_PREFIX_EXPRESSION: {
            struct AST_PREFIX_EXPRESSION expr = node->data.AST_PREFIX_EXPRESSION;

            ResultType(CometType, ErrorMessage) right = resolveType(c, expr.right);
            if (right.error)
                return right;

            switch (expr.op.type) {
                case CT_NOT:
                    return Success(CometType, ErrorMessage, {.typeKind = COMET_BOOL});
                case CT_HASH:
                    return Success(CometType, ErrorMessage, {.typeKind = COMET_INT});
                
                default: {
                    Estr buffer = CREATE_ESTR("Invalid prefix operator: \"");
                    APPEND_ESTR(buffer, tokenTypeToCStr(expr.op.type));
                    APPEND_ESTR(buffer, "\"");

                    ErrorMessage errMsg = createError(
                        c->inputFilePath,
                        c->sourceCode,
                        "InvalidOperator",
                        buffer.str,
                        NULL,
                        node->lineNum,
                        node->startCol,
                        node->endCol
                    );

                    return Error(CometType, ErrorMessage, errMsg);
                }
            }
        }

        case AST_INFIX_EXPRESSION: {
            struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

            ResultType(CometType, ErrorMessage) left = resolveType(c, expr.left);
            if (left.error)
                return left;

            switch (expr.op.type) {
                case CT_DIVIDE: // division always results in a double
                    return Success(CometType, ErrorMessage, {.typeKind = COMET_DOUBLE});

                case CT_COLON:
                    if (left.as.success.typeKind != COMET_ARRAY) {
                        ErrorMessage errMsg = createError(
                            c->inputFilePath,
                            c->sourceCode,
                            "TypeError",
                            "Attempted to index something that isn't an array!",
                            NULL,
                            node->lineNum,
                            node->startCol,
                            node->endCol
                        );

                        return Error(CometType, ErrorMessage, errMsg);
                    }

                    return Success(CometType, ErrorMessage, getTopArrayElemType(left.as.success.arrayType));

                case CT_DOT: { // get type of field
                    if (left.as.success.typeKind == COMET_MODULE)
                        return getModuleAttribType(c, node);
                    else if (left.as.success.typeKind != COMET_STRUCT) {
                        ErrorMessage errMsg = createError(
                            c->inputFilePath,
                            c->sourceCode,
                            "TypeError",
                            "Attempted to get a field of something that isn't a struct!",
                            NULL,
                            node->lineNum,
                            node->startCol,
                            node->endCol
                        );

                        return Error(CometType, ErrorMessage, errMsg);
                    }
                    
                    char* fieldName = expr.right->data.AST_IDENTIFIER.ident;

                    ResultType(CometFunctionTypeInfo, ErrorMessage) funcInfo = getFunction(c, node, false);
                    if (!funcInfo.error) {
                        // resolving type of method
                        int32_t methodIdx = getMethodIndex(left.as.success.structType, fieldName);
                        if (methodIdx == -1) {
                            Estr buffer = CREATE_ESTR("No such method \"");
                            APPEND_ESTR(buffer, fieldName);
                            APPEND_ESTR(buffer, "\" in struct \"");
                            APPEND_ESTR(buffer, left.as.success.structType->name);
                            APPEND_ESTR(buffer, "\"");

                            ErrorMessage errMsg = createError(
                                c->inputFilePath,
                                c->sourceCode,
                                "UndefinedMethod",
                                buffer.str,
                                NULL,
                                node->lineNum,
                                node->startCol,
                                node->endCol
                            );

                            return Error(CometType, ErrorMessage, errMsg);
                        }

                        CometMethod* method = left.as.success.structType->vtable[methodIdx];
                        CometFunction* func = c->functions[method->symbolIdx];

                        CometType funcType = {
                            .typeKind = COMET_FUNCTION,
                            .functionType = func
                        };

                        return Success(CometType, ErrorMessage, funcType);
                    }

                    
                    int32_t fieldIdx = getFieldIndex(left.as.success.structType, fieldName);
                    if (fieldIdx == -1) {
                        Estr buffer = CREATE_ESTR("No such field \"");
                        APPEND_ESTR(buffer, fieldName);
                        APPEND_ESTR(buffer, "\" in struct \"");
                        APPEND_ESTR(buffer, left.as.success.structType->name);
                        APPEND_ESTR(buffer, "\"");

                        ErrorMessage errMsg = createError(
                            c->inputFilePath,
                            c->sourceCode,
                            "UndefinedField",
                            buffer.str,
                            NULL,
                            node->lineNum,
                            node->startCol,
                            node->endCol
                        );

                        return Error(CometType, ErrorMessage, errMsg);
                    }

                    CometType fieldType = left.as.success.structType->fieldTypes[fieldIdx];

                    return Success(CometType, ErrorMessage, fieldType);
                }
                
                default: break;
            }

            

            ResultType(CometType, ErrorMessage) right = resolveType(c, expr.right);

            return Success(CometType, ErrorMessage, unifyType(left.as.success, right.as.success));
        }
        case AST_ARG_DEF: {
            return getType(c, node->data.AST_ARG_DEF.type);
        }
        case AST_FUNC_CALL: {
            ResultType(CometFunctionTypeInfo, ErrorMessage) funcResult = getFunction(c, node->data.AST_FUNC_CALL.ident, false);
            if (funcResult.error)
                return Error(CometType, ErrorMessage, funcResult.as.error);

            CometFunctionTypeInfo funcInfo = funcResult.as.success;
            CometFunction* funcSymbol = c->functions[funcInfo.value.symbolIdx];

            return Success(CometType, ErrorMessage, funcSymbol->returnType);
        }

        default: {
            Estr buffer = CREATE_ESTR("Could not resolve type of expression: \"");
            APPEND_ESTR(buffer, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(buffer, "\"");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "CompilerIssue",
                buffer.str,
                NULL,
                node->lineNum,
                node->startCol,
                node->endCol
            );

            return Error(CometType, ErrorMessage, errMsg);
        }
    }

    CometType outType = {
        .typeKind = outTypeKind
    };

    return Success(CometType, ErrorMessage, outType);
}

// -- VISIT METHODS -- //
ResultType(CometOperand, ErrorMessage) visitExpressionStatement(CometCompiler* c, CometASTNode* node) {
    return compile(c, node->data.AST_EXPRESSION_STATEMENT.expression);
}

ResultType(CometOperand, ErrorMessage) visitAssignStatement(CometCompiler* c, CometASTNode* node) {
    CometASTNode* expr = node->data.AST_ASSIGN_STATEMENT.expression;
    char* ident = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;

    c->currentLine = node->lineNum;

    Record* existingVar = lookup(c->env, ident);
    if (existingVar) {
        Estr buffer = CREATE_ESTR("Redefinition of \"");
        APPEND_ESTR(buffer, ident);
        APPEND_ESTR(buffer, "\"")

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "VariableRedefinition",
            buffer.str,
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    ResultType(CometType, ErrorMessage) varType = getType(c, node->data.AST_ASSIGN_STATEMENT.type);
    if (varType.error)
        return Error(CometOperand, ErrorMessage, varType.as.error);

    if (!expr) { // no value was given, just give it a default value of 0
        CometOperand zeroVal = createOperand(CO_IMMEDIATE);
        zeroVal.imm.typeKind = COMET_BIG;
        zeroVal.imm.bigVal = 0;

        CometOperand zeroConst = storeConst(c, zeroVal);
        buildPushConst(c, zeroConst);

        uint32_t idx = defineVar(c->env, ident, RECORD_LOCAL, zeroConst, varType.as.success, node->data.AST_ASSIGN_STATEMENT.isMutable);
        buildStore(c, idx);
        return Success(CometOperand, ErrorMessage, NO_OPERAND);
    }

    

    ResultType(CometType, ErrorMessage) exprType = resolveType(c, expr);
    if (exprType.error)
        return Error(CometOperand, ErrorMessage, exprType.as.error);

    

    ResultType(CometOperand, ErrorMessage) exprResult = visitValue(c, expr);
    if (exprResult.error)
        return exprResult;

    if (!typesAreEqual(exprType.as.success, varType.as.success) && !canImplicitCastType(varType.as.success, exprType.as.success)) {

        CometType castOut = buildCast(c, exprType.as.success, varType.as.success);

        if (typesAreEqual(exprType.as.success, castOut)) { // cast didnt do anything
            Estr help = CREATE_ESTR("Variable is type ");
            APPEND_ESTR(help, typeToString(varType.as.success));
            APPEND_ESTR(help, " but expression is type ");
            APPEND_ESTR(help, typeToString(exprType.as.success));

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "TypeMismatch",
                "Variable type and expression type don't match in assignment.",
                help.str,
                node->lineNum,
                expr->startCol,
                expr->endCol
            );

            return Error(CometOperand, ErrorMessage, errMsg);
        }
    }

    uint32_t idx = defineVar(c->env, ident, RECORD_LOCAL, exprResult.as.success, exprType.as.success, node->data.AST_ASSIGN_STATEMENT.isMutable);
    buildStore(c, idx);

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitFieldReassignStatement(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    struct AST_INFIX_EXPRESSION expr = node->data.AST_REASSIGN_STATEMENT.ident->data.AST_INFIX_EXPRESSION;

    ResultType(voidPtr, ErrorMessage) structResult = visitLValue(c, expr.left);
    if (structResult.error)
        return Error(CometOperand, ErrorMessage, structResult.as.error);

    ResultType(CometType, ErrorMessage) structType = resolveType(c, expr.left);
    if (structType.error)
        return Error(CometOperand, ErrorMessage, structType.as.error);

    if (structType.as.success.typeKind != COMET_STRUCT) {
        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "TypeError",
            "Attempted to reassign a field of something that isn't a struct",
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    int32_t fieldIndex = getFieldIndex(structType.as.success.structType, expr.right->data.AST_IDENTIFIER.ident);
    ResultType(CometType, ErrorMessage) exprType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.expression);
    if (exprType.error)
        return Error(CometOperand, ErrorMessage, exprType.as.error);

    ResultType(CometType, ErrorMessage) varType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.ident);
    if (varType.error) 
        return Error(CometOperand, ErrorMessage, varType.as.error);
    
    if (node->data.AST_REASSIGN_STATEMENT.op.type != CT_EQ) {
        buildGetField(c, fieldIndex);
    }

    CometType resultType = unifyType(varType.as.success, exprType.as.success);
    if (resultType.typeKind != varType.as.success.typeKind) {
        Estr buffer = CREATE_ESTR("Attempted to reassign type of field in struct \"");
        APPEND_ESTR(buffer, structType.as.success.structType->name);
        APPEND_ESTR(buffer, "\" at runtime!");

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "TypeMismatch",
            buffer.str,
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    switch (node->data.AST_REASSIGN_STATEMENT.op.type) {
        case CT_PLUS_EQ: {
            buildAdd(c, resultType);
            break;
        }
        case CT_MINUS_EQ: {
            buildSub(c, resultType);
            break;
        }
        case CT_TIMES_EQ: {
            buildMul(c, resultType);
            break;
        }
        case CT_DIVIDE_EQ: {
            buildDiv(c, resultType);
            break;
        }
        case CT_EQ: break;
        default: {
            

            Estr buffer = CREATE_ESTR("Cannot use operator \"");
            APPEND_ESTR(buffer, tokenTypeToCStr(expr.op.type));
            APPEND_ESTR(buffer, "\" to reassign field of struct \"");
            APPEND_ESTR(buffer, structType.as.success.structType->name);
            APPEND_ESTR(buffer, "\".");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "InvalidOperator",
                buffer.str,
                NULL,
                node->lineNum,
                node->startCol,
                node->endCol
            );

            return Error(CometOperand, ErrorMessage, errMsg);
        }
    }

    if (node->data.AST_REASSIGN_STATEMENT.op.type != CT_EQ) {
        visitLValue(c, expr.left);
    }

    buildSetField(c, fieldIndex);

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitArrayReassignStatement(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    struct AST_INFIX_EXPRESSION expr = node->data.AST_REASSIGN_STATEMENT.ident->data.AST_INFIX_EXPRESSION;

    ResultType(voidPtr, ErrorMessage) arrayResult = visitLValue(c, node->data.AST_REASSIGN_STATEMENT.ident);
    if (arrayResult.error)
        return Error(CometOperand, ErrorMessage, arrayResult.as.error);

    ResultType(CometType, ErrorMessage) arrayType = resolveType(c, expr.left);
    if (arrayType.error)
        return Error(CometOperand, ErrorMessage, arrayType.as.error);

    if (arrayType.as.success.typeKind != COMET_ARRAY) {
        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "TypeError",
            "Attempted to reassign an element of something that isn't an array!",
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    ResultType(CometType, ErrorMessage) exprType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.expression);
    if (exprType.error)
        return Error(CometOperand, ErrorMessage, exprType.as.error);

    ResultType(CometType, ErrorMessage) varType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.ident);
    if (varType.error) 
        return Error(CometOperand, ErrorMessage, varType.as.error);
    
    if (node->data.AST_REASSIGN_STATEMENT.op.type != CT_EQ) {
        buildListAt(c);
    }

    CometType resultType = unifyType(varType.as.success, exprType.as.success);
    if (resultType.typeKind != varType.as.success.typeKind) {
        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "TypeMismatch",
            "Attempted to reassign type of element in array at runtime!",
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    switch (node->data.AST_REASSIGN_STATEMENT.op.type) {
        case CT_PLUS_EQ: {
            buildAdd(c, resultType);
            break;
        }
        case CT_MINUS_EQ: {
            buildSub(c, resultType);
            break;
        }
        case CT_TIMES_EQ: {
            buildMul(c, resultType);
            break;
        }
        case CT_DIVIDE_EQ: {
            buildDiv(c, resultType);
            break;
        }
        case CT_EQ: break;
        default: {
            

            Estr buffer = CREATE_ESTR("Cannot use operator \"");
            APPEND_ESTR(buffer, tokenTypeToCStr(expr.op.type));
            APPEND_ESTR(buffer, "\" to reassign element of array!");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "InvalidOperator",
                buffer.str,
                NULL,
                node->lineNum,
                node->startCol,
                node->endCol
            );

            return Error(CometOperand, ErrorMessage, errMsg);
        }
    }

    if (node->data.AST_REASSIGN_STATEMENT.op.type != CT_EQ) {
        visitLValue(c, expr.left);
    }

    buildListSet(c);

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitReassignStatement(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;

    ResultType(CometOperand, ErrorMessage) exprResult = visitValue(c, node->data.AST_ASSIGN_STATEMENT.expression);
    if (exprResult.error)
        return exprResult;

    if (node->data.AST_REASSIGN_STATEMENT.ident->nodeType == AST_INFIX_EXPRESSION) { // infix reassign

        struct AST_INFIX_EXPRESSION leftExpr = node->data.AST_REASSIGN_STATEMENT.ident->data.AST_INFIX_EXPRESSION;

        if (leftExpr.op.type == CT_DOT) { // struct reassign
            return visitFieldReassignStatement(c, node);
        } else if (leftExpr.op.type == CT_COLON) {
            return visitArrayReassignStatement(c, node);
        } else {
            Estr buffer = CREATE_ESTR("Cannot use operator \"");
            APPEND_ESTR(buffer, tokenTypeToCStr(leftExpr.op.type));
            APPEND_ESTR(buffer, "\" in reassignment.");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "InvalidOperator",
                buffer.str,
                NULL,
                node->lineNum,
                node->startCol,
                node->endCol
            );

            return Error(CometOperand, ErrorMessage, errMsg);
        }
        
    }

    ResultType(CometType, ErrorMessage) varType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.ident);
    if (varType.error)
        return Error(CometOperand, ErrorMessage, varType.as.error);

    char* ident = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;

    Record* varRecord = lookup(c->env, ident);
    if (!varRecord) {
        Estr buffer = CREATE_ESTR("Cannot reassign undefined variable \"");
        APPEND_ESTR(buffer, ident);
        APPEND_ESTR(buffer, "\"");

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "UndefinedVariable",
            buffer.str,
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    if (!varRecord->isMutable) {
        Estr buffer = CREATE_ESTR("Cannot change value of immutable variable \"");
        APPEND_ESTR(buffer, ident);
        APPEND_ESTR(buffer, "\"");

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "ImmutableReassignment",
            buffer.str,
            "Add \"mut\" to the variable definition to make this variable mutable.",
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    if (node->data.AST_REASSIGN_STATEMENT.op.type != CT_EQ) {
        buildLoad(c, varRecord->recordIdx);
    }

    ResultType(CometType, ErrorMessage) exprType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.expression);
    if (exprType.error)
        return Error(CometOperand, ErrorMessage, exprType.as.error);

    CometType resultType = unifyType(varType.as.success, exprType.as.success);
    if (resultType.typeKind != varType.as.success.typeKind) {
        Estr buffer = CREATE_ESTR("Attempted to reassign type of variable \"");
        APPEND_ESTR(buffer, ident);
        APPEND_ESTR(buffer, "\" at runtime!");

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "TypeMismatch",
            buffer.str,
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }


    

    switch (node->data.AST_REASSIGN_STATEMENT.op.type) {
        case CT_PLUS_EQ: {
            buildAdd(c, resultType);
            break;
        }
        case CT_MINUS_EQ: {
            buildSub(c, resultType);
            break;
        }
        case CT_TIMES_EQ: {
            buildMul(c, resultType);
            break;
        }
        case CT_DIVIDE_EQ: {
            buildDiv(c, resultType);
            break;
        }
        case CT_EQ: break;
        default: {
            

            Estr buffer = CREATE_ESTR("Cannot use operator \"");
            APPEND_ESTR(buffer, tokenTypeToCStr(node->data.AST_REASSIGN_STATEMENT.op.type));
            APPEND_ESTR(buffer, "\" to reassign varialbe \"");
            APPEND_ESTR(buffer, ident);
            APPEND_ESTR(buffer, "\".");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "TypeMismatch",
                buffer.str,
                NULL,
                node->lineNum,
                node->startCol,
                node->endCol
            );

            return Error(CometOperand, ErrorMessage, errMsg);
        }
    }

    buildStore(c, varRecord->recordIdx);
    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}

ResultType(CometOperand, ErrorMessage) getField(CometCompiler* c, CometASTNode* structToGet, CometASTNode* field) {
    c->currentLine = structToGet->lineNum;
    char* fieldName = field->data.AST_IDENTIFIER.ident;

    ResultType(CometType, ErrorMessage) structType = resolveType(c, structToGet);
    if (structType.error)
        return Error(CometOperand, ErrorMessage, structType.as.error);

    ResultType(CometOperand, ErrorMessage) structValue = visitValue(c, structToGet);
    if (structValue.error) 
        return structValue;

    int32_t fieldIdx = getFieldIndex(structType.as.success.structType, fieldName);
    if (fieldIdx == -1) {
        Estr buffer = CREATE_ESTR("Struct \"");
        APPEND_ESTR(buffer, typeToString(structType.as.success));
        APPEND_ESTR(buffer, "\" doesn't have a field named \"");
        APPEND_ESTR(buffer, fieldName);
        APPEND_ESTR(buffer, "\"");

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "FieldNotFound",
            buffer.str,
            NULL,
            field->lineNum,
            field->startCol,
            field->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    CometOperand dest = buildGetField(c, fieldIdx);

    return Success(CometOperand, ErrorMessage, dest);
}
ResultType(CometOperand, ErrorMessage) visitInfixExpression(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

    ResultType(CometType, ErrorMessage) leftType = resolveType(c, expr.left);
    if (leftType.error)
        return Error(CometOperand, ErrorMessage, leftType.as.error);

    if (expr.op.type == CT_DOT) { // getting a field
        if (leftType.as.success.typeKind == COMET_MODULE)
            return getModuleValue(c, node);

        return getField(c, expr.left, expr.right);
    }

    ResultType(CometType, ErrorMessage) rightType = resolveType(c, expr.right);
    if (rightType.error)
        return Error(CometOperand, ErrorMessage, rightType.as.error);

    CometType resultType = unifyType(leftType.as.success, rightType.as.success);
    
    // left
    ResultType(CometOperand, ErrorMessage) leftValue = visitValue(c, expr.left);
    if (leftValue.error)
        return leftValue;

    if (typesAreEqual(leftType.as.success, resultType)) {
        leftType.as.success = buildCast(c, leftType.as.success, resultType);
    }

    // right
    ResultType(CometOperand, ErrorMessage) rightValue = visitValue(c, expr.right);
    if (rightValue.error)
        return rightValue;

    if (typesAreEqual(rightType.as.success, resultType)) {
        rightType.as.success = buildCast(c, rightType.as.success, resultType);
    }

    CometOperand out;

    // int operations
    switch (expr.op.type) {
        // arithmetic
        case CT_PLUS: {
            out = buildAdd(c, resultType);
            break;
        }
        case CT_MINUS: {
            out = buildSub(c, resultType);
            break;
        }
        case CT_TIMES: {
            out = buildMul(c, resultType);
            break;
        }
        case CT_DIVIDE: {
            out = buildDiv(c, resultType);
            break;
        }

        // conditionals
        case CT_EQ_EQ: {
            out = buildEq(c, resultType);
            break;
        }
        case CT_NOT_EQ: {
            out = buildNeq(c, resultType);
            break;
        }
        case CT_LT: {
            out = buildLt(c, resultType);
            break;
        }
        case CT_GT: {
            out = buildGt(c, resultType);
            break;
        }
        case CT_LTE: {
            out = buildLte(c, resultType);
            break;
        }
        case CT_GTE: {
            out = buildGte(c, resultType);
            break;
        }

        // list access
        case CT_COLON: {
            out = buildListAt(c);
            break;
        }

        default: {
            Estr buffer = CREATE_ESTR("Invalid operator for types: \"");
            APPEND_ESTR(buffer, tokenTypeToCStr(expr.op.type));
            APPEND_ESTR(buffer, "\"");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "InvalidOperator",
                buffer.str,
                NULL,
                node->lineNum,
                node->startCol,
                node->endCol
            );

            return Error(CometOperand, ErrorMessage, errMsg);
        }
    }

    return Success(CometOperand, ErrorMessage, out);
}

ResultType(CometOperand, ErrorMessage) visitPrefixExpression(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    struct AST_PREFIX_EXPRESSION expr = node->data.AST_PREFIX_EXPRESSION;

    ResultType(CometType, ErrorMessage) rightType = resolveType(c, expr.right);
    if (rightType.error)
        return Error(CometOperand, ErrorMessage, rightType.as.error);

    ResultType(CometOperand, ErrorMessage) rightVal = visitValue(c, expr.right);
    if (rightVal.error)
        return rightVal;

    switch (expr.op.type) {
        case CT_NOT:
            buildNot(c);
            break;

        case CT_HASH:
            if (rightType.as.success.typeKind != COMET_ARRAY) {
                Estr buffer = CREATE_ESTR("Cannot get length of type ");
                APPEND_ESTR(buffer, typeToString(rightType.as.success));

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "TypeMismatch",
                    buffer.str,
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );

                return Error(CometOperand, ErrorMessage, errMsg);
            }

            buildListLength(c);
            break;
        
        default: {
            Estr buffer = CREATE_ESTR("Invalid prefix operator: \"");
            APPEND_ESTR(buffer, tokenTypeToCStr(expr.op.type));
            APPEND_ESTR(buffer, "\"");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "InvalidOperator",
                buffer.str,
                NULL,
                node->lineNum,
                node->startCol,
                node->endCol
            );

            return Error(CometOperand, ErrorMessage, errMsg);
        }
    }

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}

ResultType(CometOperand, ErrorMessage) visitFuncDefStatement(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    struct AST_FUNC_DEF_STATEMENT funcDef = node->data.AST_FUNC_DEF_STATEMENT;
    char* funcName = funcDef.ident->data.AST_IDENTIFIER.ident;

    if (node->data.AST_FUNC_DEF_STATEMENT.args.count > MAX_ARGS) {
        char* buffer = malloc(64);
        snprintf(buffer, 64, "Functions can't have more than %d args.", MAX_ARGS);

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "MaxFunctionArgsExceeded",
            buffer,
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    // get arg types
    CometType* argTypes = calloc(funcDef.args.count, sizeof(CometType));
    for (size_t argTypeIdx = 0; argTypeIdx < funcDef.args.count; argTypeIdx++) {
        CometASTNode* argNode = *get(funcDef.args, argTypeIdx);

        ResultType(CometType, ErrorMessage) argType = getType(c, argNode->data.AST_ARG_DEF.type);
        if (argType.error)
            return Error(CometOperand, ErrorMessage, argType.as.error);

        argTypes[argTypeIdx] = argType.as.success;
    } 

    // get return type
    ResultType(CometType, ErrorMessage) returnType = getType(c, funcDef.returnType);
    if (returnType.error)
        return Error(CometOperand, ErrorMessage, returnType.as.error);

    // build the function start and define the function in the current scope
    CometOperand funcValue = buildFunction(c, funcName, funcDef.args.count, returnType.as.success, argTypes, false, false, false, -1);
    CometType funcType = {
        .typeKind = COMET_FUNCTION,
        .functionType = getValueType(c, funcValue).functionType
    };
    defineVar(c->env, funcName, RECORD_LOCAL, funcValue, funcType, false);

    // create the new scope for the function
    CometEnvironment* funcEnv = newEnvironment(funcName, c->env, true);
    c->env = funcEnv;

    // define each argument in the functions scope
    for (size_t argIdx = 0; argIdx < funcDef.args.count; argIdx++) {
        CometASTNode* argNode = *get(funcDef.args, argIdx);
        struct AST_ARG_DEF argument = argNode->data.AST_ARG_DEF;

        ResultType(CometType, ErrorMessage) argType = resolveType(c, argNode);
        if (argType.error)
            return Error(CometOperand, ErrorMessage, argType.as.error);

        CometOperand argValue = createOperand(CO_IMMEDIATE);
        argValue.imm.typeKind = COMET_SMALL;
        argValue.imm.smallVal = argIdx;

        defineVar(
            c->env,
            argument.ident->data.AST_IDENTIFIER.ident, 
            RECORD_ARG,
            argValue, 
            argType.as.success,
            false
        );
    }

    // build the functions body
    ResultType(CometOperand, ErrorMessage) bodyResult = compile(c, funcDef.program);
    if (bodyResult.error)
        return bodyResult;

    // return back to the parent scope
    c->env = destroyEnv(c->env);
    endBlock(c);

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitReturnStatement(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    if (c->currentFunction->returnType.typeKind != COMET_VOID) {
        ResultType(CometOperand, ErrorMessage) returnValue = visitValue(c, node->data.AST_RETURN_STATEMENT.expression);
        if (returnValue.error)
            return returnValue;
    }
    
    buildReturn(c);

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitFuncCall(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    struct AST_FUNC_CALL funcCall = node->data.AST_FUNC_CALL;

    ResultType(CometType, ErrorMessage) funcParentType = resolveType(c, funcCall.ident);
    if (funcParentType.error)
        return Error(CometOperand, ErrorMessage, funcParentType.as.error);

    ResultType(CometFunctionTypeInfo, ErrorMessage) funcVal = getFunction(c, funcCall.ident, false);
    if (funcVal.error)
        return Error(CometOperand, ErrorMessage, funcVal.as.error);

    CometFunction* func = c->functions[funcVal.as.success.value.symbolIdx];
    uint32_t neededArgCount = func->isMethod ? func->argCount - 1 : func->argCount;

    if (funcCall.args.count < neededArgCount) {
        Estr buffer = CREATE_ESTR("Not enough args passed to function \"");
        APPEND_ESTR(buffer, func->name);
        APPEND_ESTR(buffer, "\"");

        char* help = malloc(64);
        snprintf(help, 64, "Function expects %d args but got %zu.", neededArgCount, funcCall.args.count);

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "NotEnoughArgs",
            buffer.str,
            help,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    } else if (funcCall.args.count > neededArgCount && !func->isVarArgs) {
        Estr buffer = CREATE_ESTR("Too many args passed to function \"");
        APPEND_ESTR(buffer, func->name);
        APPEND_ESTR(buffer, "\"");

        char* help = malloc(64);
        snprintf(help, 64, "Function expects %d args but got %zu.", neededArgCount, funcCall.args.count);

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "TooManyArgs",
            buffer.str,
            help,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    List(CometOperand) funcCallArgs = newList(CometOperand);
    for (size_t argIdx = 0; argIdx < funcCall.args.count; argIdx++) {
        size_t actualArgIdx = func->isMethod ? argIdx + 1 : argIdx;

        CometASTNode* argNode = *get(funcCall.args, argIdx);

        ResultType(CometOperand, ErrorMessage) argValue = visitValue(c, argNode);
        if (argValue.error)
            return argValue;

        ResultType(CometType, ErrorMessage) argType = resolveType(c, argNode);
        if (argType.error)
            return Error(CometOperand, ErrorMessage, argType.as.error);

        if (argIdx < neededArgCount &&
            !typesAreEqual(argType.as.success, func->argTypes[actualArgIdx]) &&
            !canImplicitCastType(func->argTypes[actualArgIdx], argType.as.success)) {
            char* buffer = malloc(128);
            snprintf(buffer, 128, "Argument %zu of function \"%s\" is the wrong type", actualArgIdx+1, func->name);

            char* help = malloc(256);
            snprintf(
                help,
                256,
                "Expected %s but got %s. Full function signature is %s",
                typeToString(func->argTypes[actualArgIdx]),
                typeToString(argType.as.success),
                typeToString((CometType){ .typeKind = COMET_FUNCTION, .functionType = func })
            );

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "TypeMismatch",
                buffer,
                help,
                argNode->lineNum,
                argNode->startCol,
                argNode->endCol
            );

            return Error(CometOperand, ErrorMessage, errMsg);
        }

        append(funcCallArgs, argValue.as.success);
    }

    funcVal = getFunction(c, funcCall.ident, true);

    CometOperand returnValue;
    if (funcVal.as.success.funcType == FUNC_FUNC) {
        returnValue = buildCall(c, func->name, funcCallArgs);
    } else { // FUNC_METHOD
        returnValue = buildCallMethod(c, funcVal.as.success.methodIdx.imm.smallVal, funcCallArgs);
    }

    return Success(CometOperand, ErrorMessage, returnValue);
}
ResultType(CometOperand, ErrorMessage) visitIfStatement(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    struct AST_IF_STATEMENT ifStmt = node->data.AST_IF_STATEMENT;
    CometASTNode* elseBody = ifStmt.elseProgram;

    CometLabel* endLabel = buildLabel(c);
    CometLabel* elseLabel = buildLabel(c);

    ResultType(CometOperand, ErrorMessage) condition = visitValue(c, ifStmt.expression);
    if (condition.error)
        return condition;

    if (elseBody != NULL)
        buildJumpIfFalse(c, elseLabel);
    else
        buildJumpIfFalse(c, endLabel);

    ResultType(CometOperand, ErrorMessage) ifBodyResult = compile(c, ifStmt.program);
    if (ifBodyResult.error)
        return ifBodyResult;

    

    

    if (ifStmt.elseProgram != NULL) {
        buildJump(c, endLabel);
        resolveLabel(c, elseLabel);
        condition = visitValue(c, ifStmt.expression);
        if (condition.error)
            return condition;

        buildNot(c);
        buildJumpIfFalse(c, endLabel);

        ResultType(CometOperand, ErrorMessage) elseBodyResult = compile(c, ifStmt.elseProgram);
        if (elseBodyResult.error)
            return elseBodyResult;

    }

    resolveLabel(c, endLabel);
    

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitWhileStatement(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    struct AST_WHILE_STATEMENT whileStmt = node->data.AST_WHILE_STATEMENT;
    
    CometLabel* startLabel = buildLabel(c);
    CometLabel* endLabel = buildLabel(c);

    CometEnvironment* whileEnv = newEnvironment("whileLoop", c->env, false);
    c->env = whileEnv;

    resolveLabel(c, startLabel);
    ResultType(CometOperand, ErrorMessage) condition = visitValue(c, whileStmt.expression);
    if (condition.error)
        return condition;

    buildJumpIfFalse(c, endLabel);

    ResultType(CometOperand, ErrorMessage) whileBodyResult = compile(c, whileStmt.program);
    if (whileBodyResult.error)
        return whileBodyResult;

    buildJump(c, startLabel);
    resolveLabel(c, endLabel);

    c->env = destroyEnv(whileEnv);

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitForStatement(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    struct AST_FOR_STATEMENT forStmt = node->data.AST_FOR_STATEMENT;

    CometLabel* mainLabel = buildLabel(c);
    CometLabel* endLabel = buildLabel(c);

    // resolve start and end types
    ResultType(CometType, ErrorMessage) startType = resolveType(c, forStmt.start);
    if (startType.error)
        return Error(CometOperand, ErrorMessage, startType.as.error);
    ResultType(CometType, ErrorMessage) endType = resolveType(c, forStmt.end);
    if (endType.error)
        return Error(CometOperand, ErrorMessage, endType.as.error);
    CometType resultType = unifyType(startType.as.success, endType.as.success);

    char* ident = forStmt.ident->data.AST_IDENTIFIER.ident;

    // create env for for loop
    CometEnvironment* forLoopEnv = newEnvironment("forLoop", c->env, false);
    c->env = forLoopEnv;

    // define iterator variable
    ResultType(CometOperand, ErrorMessage) start = visitValue(c, forStmt.start);
    if (start.error)
        return Error(CometOperand, ErrorMessage, start.as.error);

    uint32_t idx = defineVar(c->env, ident, RECORD_LOCAL, start.as.success, resultType, false);
    buildStore(c, idx);

    resolveLabel(c, mainLabel);

    // if the iterator var is equal to the end value, then we jump to the exit of the for loop
    buildLoad(c, idx);

    ResultType(CometOperand, ErrorMessage) end  = visitValue(c, forStmt.end);
    if (end.error)
        return Error(CometOperand, ErrorMessage, end.as.error);

    buildNeq(c, startType.as.success);
    buildJumpIfFalse(c, endLabel);

    // compile the body of the for loop
    ResultType(CometOperand, ErrorMessage) bodyResult = compile(c, forStmt.program);
    if (bodyResult.error)
        return bodyResult;

    // compile the step value
    ResultType(CometType, ErrorMessage) stepType = resolveType(c, forStmt.step);
    if (stepType.error)
        return Error(CometOperand, ErrorMessage, stepType.as.error);

    buildLoad(c, idx);

    ResultType(CometOperand, ErrorMessage) step = visitValue(c, forStmt.step);
    if (step.error)
        return Error(CometOperand, ErrorMessage, step.as.error);

    CometType addType = unifyType(startType.as.success, stepType.as.success);

    // add the step to the iterator var
    buildAdd(c, addType);

    // save the iterator value
    buildStore(c, idx);

    // jump back to the start of the for loop
    buildJump(c, mainLabel);

    // resolve label for end of loop
    resolveLabel(c, endLabel);

    // exit the for loop's env
    c->env = destroyEnv(forLoopEnv);

    

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitConstructorDefStatement(CometCompiler* c, CometASTNode* node, char* constructorName, CometType structType, CometStruct* parentStruct) {
    c->currentLine = node->lineNum;
    struct AST_CONSTRUCTOR_DEF constDef = node->data.AST_CONSTRUCTOR_DEF;

    // get arg types
    CometType* argTypes = calloc(constDef.args.count + 1, sizeof(CometType));
    for (size_t argTypeIdx = 0; argTypeIdx < constDef.args.count; argTypeIdx++) {
        CometASTNode* argNode = *get(constDef.args, argTypeIdx);

        ResultType(CometType, ErrorMessage) argType = getType(c, argNode->data.AST_ARG_DEF.type);
        if (argType.error)
            return Error(CometOperand, ErrorMessage, argType.as.error);

        argTypes[argTypeIdx+1] = argType.as.success;
    }
    argTypes[0] = structType;

    buildFunction(c, constructorName, constDef.args.count + 1, structType, argTypes, false, true, false, -1); // add 1 arg for self

    // create the new scope for the function
    CometEnvironment* funcEnv = newEnvironment(constructorName, c->env, true);
    c->env = funcEnv;

     // define self
    CometOperand selfValue = createOperand(CO_IMMEDIATE);
    selfValue.imm.typeKind = COMET_SMALL;
    selfValue.imm.smallVal = 0;

    uint32_t selfIdx = defineVar(
        c->env,
        "self",
        RECORD_ARG,
        selfValue,
        structType,
        false
    );

    // define each argument in the functions scope
    for (size_t argIdx = 0; argIdx < constDef.args.count; argIdx++) {
        CometASTNode* argNode = *get(constDef.args, argIdx);
        struct AST_ARG_DEF argument = argNode->data.AST_ARG_DEF;

        ResultType(CometType, ErrorMessage) argType = resolveType(c, argNode);
        if (argType.error)
            return Error(CometOperand, ErrorMessage, argType.as.error);

        CometOperand argValue = createOperand(CO_IMMEDIATE);
        argValue.imm.typeKind = COMET_SMALL;
        argValue.imm.smallVal = argIdx;

        defineVar(
            c->env,
            argument.ident->data.AST_IDENTIFIER.ident, 
            RECORD_ARG,
            argValue, 
            argType.as.success,
            false
        );
    }

   

    // if we inherit from a struct, define parent constructor
    if (parentStruct != NULL) {
        Estr parentConstructorName = CREATE_ESTR(parentStruct->name);
        APPEND_ESTR(parentConstructorName, "_INIT");

        CometOperand superVal = createOperand(CO_SYMBOL);
        superVal.symbolIdx = getSymbolIndex(c, parentConstructorName.str);

        DESTROY_ESTR(parentConstructorName);

        CometType superValType = getValueType(c, superVal);
        superValType.functionType->isMethod = false;

        defineVar(
            c->env,
            "super",
            RECORD_ARG,
            superVal,
            superValType,
            false
        );
    }

    // build the functions body
    ResultType(CometOperand, ErrorMessage) bodyResult = compile(c, constDef.program);
    if (bodyResult.error)
        return bodyResult;

    // build return
    buildLoadArg(c, selfIdx);
    buildReturn(c);

    // return back to the parent scope
    c->env = destroyEnv(funcEnv);
    endBlock(c);

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitMethodDefStatement(CometCompiler* c, CometASTNode* node, CometType structType) {
    c->currentLine = node->lineNum;
    struct AST_FUNC_DEF_STATEMENT funcDef = node->data.AST_FUNC_DEF_STATEMENT;
    char* funcName = funcDef.ident->data.AST_IDENTIFIER.ident;

    // get arg types
    CometType* argTypes = calloc(funcDef.args.count+1, sizeof(CometType));
    argTypes[0] = structType;
    for (size_t argTypeIdx = 0; argTypeIdx < funcDef.args.count; argTypeIdx++) {
        CometASTNode* argNode = *get(funcDef.args, argTypeIdx);

        ResultType(CometType, ErrorMessage) argType = getType(c, argNode->data.AST_ARG_DEF.type);
        if (argType.error)
            return Error(CometOperand, ErrorMessage, argType.as.error);

        argTypes[argTypeIdx+1] = argType.as.success;
    } 

    // get return type
    
    ResultType(CometType, ErrorMessage) returnType = getType(c, funcDef.returnType);
    if (returnType.error)
        return Error(CometOperand, ErrorMessage, returnType.as.error);

    // build the function start and define the function in the current scope
    CometOperand funcValue = buildFunction(c, funcName, funcDef.args.count+1, returnType.as.success, argTypes, false, true, false, -1);
    CometType funcType = {
        .typeKind = COMET_FUNCTION,
        .functionType = getValueType(c, funcValue).functionType
    };
    defineVar(c->env, funcName, RECORD_LOCAL, funcValue, funcType, false);

    // create the new scope for the function
    CometEnvironment* funcEnv = newEnvironment(funcName, c->env, true);
    c->env = funcEnv;

    // define each argument in the functions scope
    size_t argIdx;
    for (argIdx = 0; argIdx < funcDef.args.count; argIdx++) {
        CometASTNode* argNode = *get(funcDef.args, argIdx);
        struct AST_ARG_DEF argument = argNode->data.AST_ARG_DEF;

        ResultType(CometType, ErrorMessage) argType = resolveType(c, argNode);
        if (argType.error)
            return Error(CometOperand, ErrorMessage, argType.as.error);

        CometOperand argValue = createOperand(CO_IMMEDIATE);
        argValue.imm.typeKind = COMET_SMALL;
        argValue.imm.smallVal = argIdx+1;

        defineVar(
            c->env,
            argument.ident->data.AST_IDENTIFIER.ident, 
            RECORD_ARG,
            argValue, 
            argType.as.success,
            false
        );
    }

    CometOperand selfVal = createOperand(CO_IMMEDIATE);
    selfVal.imm.typeKind = COMET_SMALL;
    selfVal.imm.smallVal = argIdx;

    defineVar(
        c->env,
        "self",
        RECORD_ARG,
        selfVal,
        structType,
        false
    );

    // build the functions body
    ResultType(CometOperand, ErrorMessage) bodyResult = compile(c, funcDef.program);
    if (bodyResult.error)
        return bodyResult;

    // return back to the parent scope
    c->env = destroyEnv(c->env);
    endBlock(c);

    return Success(CometOperand, ErrorMessage, funcValue);
}
ResultType(cometTypePtr, ErrorMessage) visitStructDefStatement(CometCompiler* c, CometASTNode* node, bool isGenericInstantiation, char* genericNameEnding) {
    c->currentLine = node->lineNum;
    struct AST_STRUCT_DEF_STATEMENT structDef = node->data.AST_STRUCT_DEF_STATEMENT;

    // handle generic types
    if (structDef.genericTypes.count > 0 && !isGenericInstantiation) {
        GenericStructDef newGeneric = {
            .name = structDef.ident->data.AST_IDENTIFIER.ident,
            .structDefNode = node
        };

        append(c->genericDefinitions, newGeneric);

        return Success(cometTypePtr, ErrorMessage, NULL);
    }

    CometStruct* structType = malloc(sizeof(CometStruct));
    char* structName = structDef.ident->data.AST_IDENTIFIER.ident;

    if (genericNameEnding) {
        Estr mangledName = CREATE_ESTR(structName);
        APPEND_ESTR(mangledName, genericNameEnding);
        structName = mangledName.str;
    }

    structType->name = structName;
    structType->parent = NULL;

    // define fields
    size_t fieldCount = 0;
    size_t methodCount = 0;
    size_t myFieldCount = 0;
    size_t myMethodCount = 0;
    size_t parentFieldCount = 0;
    size_t parentMethodCount = 0;

    // handle inheritance
    CometStruct* parentStruct = NULL;
    if (structDef.parentName) {
        ResultType(CometType, ErrorMessage) parentStructType = getType(c, structDef.parentName);
        if (parentStructType.error)
            return Error(cometTypePtr, ErrorMessage, parentStructType.as.error);

        if (parentStructType.as.success.typeKind != COMET_STRUCT) {
            Estr buffer = CREATE_ESTR("Cannot inherit from non-struct type \"");
            APPEND_ESTR(buffer, structDef.parentName->data.AST_IDENTIFIER.ident);
            APPEND_ESTR(buffer, "\"");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "TypeError",
                buffer.str,
                NULL,
                node->lineNum,
                node->startCol,
                node->endCol
            );

            return Error(cometTypePtr, ErrorMessage, errMsg);
        }

        parentStruct = parentStructType.as.success.structType;

        // add parent field count
        parentFieldCount = parentStruct->fieldCount;
        parentMethodCount = parentStruct->numMethods;

        structType->parent = parentStruct;
    }

    // fill in fieldDefs
    for (size_t i = 0; i < structDef.fieldDefs.count; i++) {
        CometASTNode* fieldDef = *get(structDef.fieldDefs, i);

        switch (fieldDef->nodeType) {
            case AST_ASSIGN_STATEMENT:
                myFieldCount++;
                break;

            case AST_FUNC_DEF_STATEMENT:
                myMethodCount++;
                break;

            case AST_OVERRIDE_STATEMENT:
                break;

            default: {
                Estr buffer = CREATE_ESTR("Cannot define \"");
                APPEND_ESTR(buffer, ASTNodeTypeToCStr(fieldDef->nodeType));
                APPEND_ESTR(buffer, "\" in struct.");

                ErrorMessage errMsg = createError(
                    c->inputFilePath,
                    c->sourceCode,
                    "TypeError",
                    buffer.str,
                    NULL,
                    node->lineNum,
                    node->startCol,
                    node->endCol
                );

                return Error(cometTypePtr, ErrorMessage, errMsg);
            }
        }
    }
    fieldCount += myFieldCount + parentFieldCount;
    methodCount += myMethodCount + parentMethodCount;

    structType->fieldCount = fieldCount;
    structType->numMethods = methodCount;
    structType->fieldNames = calloc(structType->fieldCount, sizeof(char*));
    structType->fieldTypes = calloc(structType->fieldCount, sizeof(CometType));
    structType->vtable = calloc(structType->numMethods, sizeof(CometMethod*));

    CometType generalStructType = {
        .typeKind = COMET_STRUCT,
        .structType = structType
    };

    // if we inherit from another struct then pull in its methods and fields
    for (size_t i = 0; i < parentFieldCount; i++) {
        structType->fieldNames[i] = parentStruct->fieldNames[i];
        structType->fieldTypes[i] = parentStruct->fieldTypes[i];
    }
    for (size_t i = 0; i < parentMethodCount; i++) {
        
        structType->vtable[i] = parentStruct->vtable[i];
    }

    uint32_t vtableIdx = parentMethodCount;
    uint32_t fieldIdx = parentFieldCount;
    for (size_t i = 0; i < structDef.fieldDefs.count; i++) {
        CometASTNode* fieldDef = *get(structDef.fieldDefs, i);

        switch (fieldDef->nodeType) {
            case AST_ASSIGN_STATEMENT: {
                ResultType(CometType, ErrorMessage) fieldType = getType(c, fieldDef->data.AST_ASSIGN_STATEMENT.type);
                if (fieldType.error)
                    return Error(cometTypePtr, ErrorMessage, fieldType.as.error);

                structType->fieldNames[fieldIdx] = fieldDef->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;
                structType->fieldTypes[fieldIdx++] = fieldType.as.success;
                break;
            }

            case AST_OVERRIDE_STATEMENT: {
                // we're not inheriting from any struct so we cant override functions
                if (parentStruct == NULL) {
                    Estr buffer = CREATE_ESTR("Cannot use an override statement in struct \"");
                    APPEND_ESTR(buffer, structName);
                    APPEND_ESTR(buffer, "\" because it has no parent.");

                    ErrorMessage errMsg = createError(
                        c->inputFilePath,
                        c->sourceCode,
                        "SemanticError",
                        buffer.str,
                        NULL,
                        node->lineNum,
                        node->startCol,
                        node->endCol
                    );

                    return Error(cometTypePtr, ErrorMessage, errMsg);
                }

                ResultType(CometOperand, ErrorMessage) result = visitMethodDefStatement(c, fieldDef->data.AST_OVERRIDE_STATEMENT.funcDef, generalStructType);
                if (result.error)
                    return Error(cometTypePtr, ErrorMessage, result.as.error);

                CometFunction* function = c->functions[result.as.success.symbolIdx];
                int32_t parentMethodIdx = getMethodIndex(parentStruct, function->name);
            
                // overriding a function that doesn't exist in the parent
                if (parentMethodIdx == -1) {
                    Estr buffer = CREATE_ESTR("Cannot override method \"");
                    APPEND_ESTR(buffer, function->name);
                    APPEND_ESTR(buffer, "\" because parent struct doesn't have it.");

                    ErrorMessage errMsg = createError(
                        c->inputFilePath,
                        c->sourceCode,
                        "SemanticError",
                        buffer.str,
                        NULL,
                        node->lineNum,
                        node->startCol,
                        node->endCol
                    );

                    return Error(cometTypePtr, ErrorMessage, errMsg);
                }

                Estr newFuncName = CREATE_ESTR(structName);
                APPEND_ESTR(newFuncName, "_");
                APPEND_ESTR(newFuncName, function->name);

                CometMethod* newMethod = malloc(sizeof(CometMethod));
                memcpy(newMethod->name, function->name, strlen(function->name) + 1);
                memcpy(function->name, newFuncName.str, newFuncName.size + 1);
                newMethod->argCount = function->argCount;
                newMethod->blockIdx = function->blockIdx,
                newMethod->symbolIdx = result.as.success.symbolIdx;

                DESTROY_ESTR(newFuncName);

                structType->vtable[parentMethodIdx] = newMethod;
                break;
            }

            case AST_FUNC_DEF_STATEMENT: {

                ResultType(CometOperand, ErrorMessage) result = visitMethodDefStatement(c, fieldDef, generalStructType);
                if (result.error)
                    return Error(cometTypePtr, ErrorMessage, result.as.error);

                CometFunction* function = c->functions[result.as.success.symbolIdx];

                Estr newFuncName = CREATE_ESTR(structName);
                APPEND_ESTR(newFuncName, "_");
                APPEND_ESTR(newFuncName, function->name);

                CometMethod* newMethod = malloc(sizeof(CometMethod));
                memcpy(newMethod->name, function->name, strlen(function->name) + 1);
                memcpy(function->name, newFuncName.str, newFuncName.size + 1);
                newMethod->argCount = function->argCount;
                newMethod->blockIdx = function->blockIdx,
                newMethod->symbolIdx = result.as.success.symbolIdx;

                DESTROY_ESTR(newFuncName);

                structType->vtable[vtableIdx++] = newMethod;
                break;
            }

            default: break;
        }
    }

    // build constructor
    if (!structDef.constructor) {
        Estr buffer = CREATE_ESTR("Struct \"");
        APPEND_ESTR(buffer, structName);
        APPEND_ESTR(buffer, "\" is missing a constructor!");

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "MissingConstructor",
            buffer.str,
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(cometTypePtr, ErrorMessage, errMsg);
    }
    Estr constructorName = CREATE_ESTR(strdup(structName));
    APPEND_ESTR(constructorName, "_INIT");

    ResultType(CometOperand, ErrorMessage) constructorResult = visitConstructorDefStatement(c, structDef.constructor, constructorName.str, generalStructType, parentStruct);
    if (constructorResult.error)
        return Error(cometTypePtr, ErrorMessage, constructorResult.as.error);

    CometType* typePtr = malloc(sizeof(CometType));

    CometType resultStructType = {
        .typeKind = COMET_STRUCT,
        .structType = structType
    };
    *typePtr = resultStructType;

    defineType(c->typeMap, structName, resultStructType);
    append(c->structs, structType);

    return Success(cometTypePtr, ErrorMessage, typePtr);
}
ResultType(CometOperand, ErrorMessage) visitNewStatement(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    struct AST_NEW_STATEMENT newStmt = node->data.AST_NEW_STATEMENT;

    // get struct type
    ResultType(CometType, ErrorMessage) structType = getType(c, newStmt.structName);
    if (structType.error)
        return Error(CometOperand, ErrorMessage, structType.as.error);
    
    char* structName = structType.as.success.structType->name;

    int32_t idx = getStructIndex(c, structType.as.success.structType);

    if (idx == -1) {
        Estr buffer = CREATE_ESTR("The type \"");
        APPEND_ESTR(buffer, structName);
        APPEND_ESTR(buffer, "\" was not found.");

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "UnkownType",
            buffer.str,
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    // make new instance
    buildNew(c, idx);

    // push args for constructor
    List(CometOperand) funcCallArgs = newList(CometOperand);
    for (size_t argIdx = 0; argIdx < newStmt.args.count; argIdx++) {
        CometASTNode* argNode = *get(newStmt.args, argIdx);

        ResultType(CometOperand, ErrorMessage) argValue = visitValue(c, argNode);
        if (argValue.error)
            return argValue;

        append(funcCallArgs, argValue.as.success);
    }

    

    // call constructor
    Estr constructorName = CREATE_ESTR(structName);
    APPEND_ESTR(constructorName, "_INIT");

    if (getSymbolIndex(c, constructorName.str) == -1) {
        Estr buffer = CREATE_ESTR("Could not find constructor for struct \"");
        APPEND_ESTR(buffer, structName);
        APPEND_ESTR(buffer, "\"");

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "CompilerIssue",
            buffer.str,
            "This is a bug. If the struct causing this error is from a library then it means the library creator forget to create a constructor.",
            node->lineNum,
            node->startCol,
            node->endCol
        );
        return Error(CometOperand, ErrorMessage, errMsg);
    }

    buildCall(c, constructorName.str, funcCallArgs);
    DESTROY_ESTR(constructorName);

    // return
    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitBreakpointStatement(CometCompiler* c) {
    buildBreakpoint(c);
    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitImportStatement(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    size_t filePathMaxLen = 1024;
    char libName[filePathMaxLen] = {};

    List(astNodePtr) importChain = node->data.AST_IMPORT_STATEMENT.importChain;

    for (size_t i = 0; i < importChain.count; i++) {
        CometASTNode* ident = *get(importChain, i);
        char* name = ident->data.AST_IDENTIFIER.ident;

        char* formatString = i < importChain.count-1 ? "%s/" : "%s";

        snprintf(libName + strlen(libName), sizeof(libName), formatString, name);
    }

    char path[filePathMaxLen] = {};
    snprintf(path, sizeof(path), "%s.comet", libName);    

    bool found = access(path, F_OK) == 0;
    bool isExternal = false;

    char* cometLibsPath = getLibsDir();
    if (!cometLibsPath) {
        cometLibsPath = "";
    }

    // no local file was found, look for it in the system wide libs
    if (!found) { 
        snprintf(path, sizeof(path), "%s/%s.comet", cometLibsPath, libName);
        found = access(path, F_OK) == 0;
    }

    // comet file was found in system wide libs, try a .cometlib file
    if (!found) { 
        snprintf(path, sizeof(path), "%s/%s.cometlib", cometLibsPath, libName);
        found = access(path, F_OK) == 0;
        isExternal = found;
    }

    // lib doesnt exist, KILL THEM!!!!
    if (!found) {
        Estr buffer = CREATE_ESTR("Could not find library \"");
        APPEND_ESTR(buffer, libName);
        APPEND_ESTR(buffer, "\"");

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "NoExternalLibFound",
            buffer.str,
            NULL,
            node->lineNum,
            node->startCol,
            node->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    char* lastModuleIdent = (*get(importChain, importChain.count-1))->data.AST_IDENTIFIER.ident;

    if (isExternal) {
        return loadExternalLib(c, path, lastModuleIdent);
    }

    // lex
    

    Estr errMsg = CREATE_ESTR("Error in imported file \"");
    APPEND_ESTR(errMsg, path);
    APPEND_ESTR(errMsg, "\":\n");

    char* fileContents = getFileContents(path);
    CometLexer lexer = newLexer(fileContents, path);
    ResultType(tokenList, ErrorMessage) tokens = lex(&lexer);
    if (tokens.error) {
        return Error(CometOperand, ErrorMessage, tokens.as.error);
    }

    ResultType(parserPtr, ErrorMessage) parser = newParser(tokens.as.success, path, fileContents);
    if (parser.error) {
        return Error(CometOperand, ErrorMessage, parser.as.error);
    }

    ResultType(astNodePtr, ErrorMessage) ast = buildAST(parser.as.success);
    if (ast.error) {
        return Error(CometOperand, ErrorMessage, ast.as.error);
    }

    ResultType(cometCompilerPtr, ErrorMessage) compiler = createCompiler(path, fileContents, c->includeDebugSymbols);
    if (compiler.error) {
        freeNode(ast.as.success);
        return Error(CometOperand, ErrorMessage, compiler.as.error);
    }

    CometEnvironment* prevEnv = c->env;
    c->env = newEnvironment(libName, c->env, false);

    // load imported symbols
    CometOperand module = createOperand(CO_IMMEDIATE);
    module.imm.typeKind = COMET_MODULE;
    module.imm.moduleVal = c->env;

    CometType moduleType = {
        .typeKind = COMET_MODULE
    };

    ResultType(CometOperand, ErrorMessage) importedCompileResult = compile(c, ast.as.success);
    if (importedCompileResult.error) {
        return importedCompileResult;
    }

    defineVar(prevEnv, lastModuleIdent, RECORD_LOCAL, module, moduleType, false);

    freeNode(ast.as.success);
    free(parser.as.success);

    c->env = prevEnv;

    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitTryStatement(CometCompiler* c, CometASTNode* node) {
    c->currentLine = node->lineNum;
    CometLabel* tryLabel = buildLabel(c);
    CometLabel* exceptLabel = buildLabel(c);
    CometLabel* endLabel = buildLabel(c);
    
    buildJump(c, tryLabel);

    // build except block
    c->env = newEnvironment("except", c->env, false);
    resolveLabel(c, exceptLabel);

    // define exception
    ResultType(CometType, ErrorMessage) exceptionType = getType(c, node->data.AST_TRY_STATEMENT.exceptionType);
    if (exceptionType.error)
        return Error(CometOperand, ErrorMessage, exceptionType.as.error);

    CometASTNode* exceptionVarNameNode = node->data.AST_TRY_STATEMENT.exceptionVarName;
    char* exceptionVarName = exceptionVarNameNode->data.AST_IDENTIFIER.ident;
    Record* existingVar = lookup(c->env, exceptionVarName);
    if (existingVar) {
        Estr buffer = CREATE_ESTR("Redefinition of \"");
        APPEND_ESTR(buffer, exceptionVarName);
        APPEND_ESTR(buffer, "\"")

        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "VariableRedefinition",
            buffer.str,
            NULL,
            exceptionVarNameNode->lineNum,
            exceptionVarNameNode->startCol,
            exceptionVarNameNode->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    uint32_t idx = defineVar(c->env, exceptionVarName, RECORD_LOCAL, NO_OPERAND, exceptionType.as.success, false);
    buildStore(c, idx);

    ResultType(CometOperand, ErrorMessage) exceptBody = compile(c, node->data.AST_TRY_STATEMENT.exceptBlock);
    if (exceptBody.error)
        return exceptBody;

    buildJump(c, endLabel);
    c->env = destroyEnv(c->env);

    // build try block
    c->env = newEnvironment("try", c->env, false);
    resolveLabel(c, tryLabel);

    // push except handler onto stack
    CometOperand exceptPos = createOperand(CO_IMMEDIATE);
    exceptPos.imm.typeKind = COMET_BIG;
    exceptPos.imm.bigVal = exceptLabel->pos;
    CometOperand exceptConst = storeConst(c, exceptPos);
    buildPushConst(c, exceptConst);

    buildTry(c);

    ResultType(CometOperand, ErrorMessage) tryBody = compile(c, node->data.AST_TRY_STATEMENT.tryBlock);
    if (tryBody.error)
        return tryBody;

    buildEndTry(c);
    
    c->env = destroyEnv(c->env);
    resolveLabel(c, endLabel);
    
    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}
ResultType(CometOperand, ErrorMessage) visitThrowStatement(CometCompiler* c, CometASTNode* node) {

    CometASTNode* exceptNode = node->data.AST_THROW_STATEMENT.newStmt;
    
    ResultType(CometOperand, ErrorMessage) execption = visitValue(c, exceptNode);
    if (execption.error)
        return execption;

    ResultType(CometType, ErrorMessage) exceptType = resolveType(c, exceptNode);
    if (exceptType.error)
        return Error(CometOperand, ErrorMessage, exceptType.as.error);

    if (exceptType.as.success.typeKind != COMET_STRUCT) {
        ErrorMessage errMsg = createError(
            c->inputFilePath,
            c->sourceCode,
            "TypeMismatch",
            "You can't throw something that isn't a struct.",
            NULL,
            exceptNode->lineNum,
            exceptNode->startCol,
            exceptNode->endCol
        );

        return Error(CometOperand, ErrorMessage, errMsg);
    }

    c->currentLine = node->lineNum;

    buildThrow(c);
    return Success(CometOperand, ErrorMessage, NO_OPERAND);
}

// -- MAIN -- //
ResultType(voidPtr, ErrorMessage) outputToFile(CometCompiler* c, const char* filePath, bool debugSymbols) {
    FILE* file = fopen(filePath, "wb");
    if (file == NULL) {
        ErrorMessage errMsg = createError(
            "<comet>",
            "<internal>",
            "FileError", 
            strerror(errno),
            NULL,
            1,
            1,
            1
        );

        return Error(voidPtr, ErrorMessage, errMsg);
    }

    uint64_t instCount = 0;
    for (size_t blockIdx = 0; blockIdx < c->blocks.count; blockIdx++) {
        // resolve labels
        for (size_t labelIdx = 0; labelIdx < c->labelCount; labelIdx++) {
            CometLabel* label = c->labels[labelIdx];
            label->pos = instCount + label->blockPos;
            label->resolved = true;
        }

        instCount += (*get(c->blocks, blockIdx)).instructions.count;
    }

    CometFile cometFile = {
        .magic = {'C', 'O', 'M', 'E',  'T'},
        .version = 1,
        .numConsts = c->constIdx,
        .numInstructions = instCount,
        .numFunctions = c->functionCount,
        .numStructs = c->structs.count,
        .numLibs = c->libs.count
    };

    fwrite(&cometFile, sizeof(CometFile), 1, file);
    fwrite(c->consts, sizeof(CometOperand), c->constIdx, file);


    for (size_t i = 0; i < c->functionCount; i++) {
        CometFunction* func = c->functions[i];
        uint32_t startIdx = 0;
        for (size_t blockIdx = 0; blockIdx < c->functions[i]->blockIdx; blockIdx++) {
            startIdx += get(c->blocks, blockIdx)->instructions.count;
        }
        

        CometSerializedFunc serializedFunc = {
            .startIdx = startIdx,
            .numArgs = func->argCount,
            .isExternal = func->isExternal,
            .libIdx = func->libIdx,
            .isVarArgs = func->isVarArgs
        };
        strcpy(serializedFunc.name, func->name);

        fwrite(&serializedFunc, sizeof(CometSerializedFunc), 1, file);
    }

    for (size_t structIdx = 0; structIdx < c->structs.count; structIdx++) {
        CometStruct* structType = *get(c->structs, structIdx);
        CometSerializedStruct* serializedStruct = serializeStruct(c->functions, structType);

        fwrite(serializedStruct->name, 1, 48, file);
        fwrite(&serializedStruct->numFields, 1, sizeof(uint32_t), file);
        fwrite(&serializedStruct->numMethods, 1, sizeof(uint32_t), file);
        fwrite(serializedStruct->vtable, sizeof(uint32_t), serializedStruct->numMethods, file);
    }

    for (size_t libIdx = 0; libIdx < c->libs.count; libIdx++) {
        char* libName = *get(c->libs, libIdx);
        fwrite(libName, 64, 1, file);
    }

    for (size_t blockIdx = 0; blockIdx < c->blocks.count; blockIdx++) {
        Block block = *get(c->blocks, blockIdx);
        for (size_t instIdx = 0; instIdx < block.instructions.count; instIdx++) {
            CometSerializedInst* inst = serializeInst(*get(block.instructions, instIdx));
            fwrite(inst, sizeof(CometSerializedInst), 1, file);
        }
    }



    fwrite(&debugSymbols, sizeof(bool), 1, file);

    if (debugSymbols) {
        char buffer[32] = {};
        snprintf(buffer, 32, "%s", c->inputFilePath);

        fwrite(buffer, 1, 32, file);
        
        size_t sourceLen = strlen(c->sourceCode) + 1;
        fwrite(&sourceLen, sizeof(size_t), 1, file);
        fwrite(c->sourceCode, 1, sourceLen, file);

        //fwrite(c->debugInstInfo.pointer, sizeof(uint64_t), c->programIdx, file);
    }

    fclose(file);


    return Success(voidPtr, ErrorMessage, NULL);
}

ResultType(int64_t, objectPtr) impl_Exception_INIT(int64_t* args, CometVM* vm) {
    CometObject* exception = (CometObject*)args[0];
    exception->fields[0] = args[1];
    exception->fields[1] = args[2];
    return Success(int64_t, objectPtr, (int64_t)exception);
}

void createExceptionType(CometCompiler* c) {
    List(StructField) fields = newList(StructField);
    StructField nameField    = { .name = "name",    .type = cometTypeString };
    StructField messageField = { .name = "message", .type = cometTypeString };
    append(fields, nameField);
    append(fields, messageField);

    List(cometFuncPtr) methods = newList(cometFuncPtr);

    CometStruct* exceptionStruct = cometDefineStruct(NULL, "Exception", NULL);
    cometSetStructFieldsAndMethods(exceptionStruct, fields, methods);

    cometDefineConstructor(NULL, exceptionStruct, 2, false, cometTypeString, cometTypeString);

    append(c->structs, exceptionStruct);


    // add to type map
    CometType exceptionType = {
        .typeKind = COMET_STRUCT,
        .structType = exceptionStruct
    };

    CometTypeMapEntry typeMapEntry = {
        .name = "Exception",
        .type = exceptionType
    };

    defineType(c->typeMap, "Exception", exceptionType);
}

void createPointerType(CometCompiler* c) {
    CometType ptrType;

    switch (sizeof(void*)) {
        case 4:
            ptrType = (CometType){.typeKind = COMET_INT };
            break;
        default:
            ptrType = (CometType){.typeKind = COMET_BIG };
            break;
    }

    defineType(c->typeMap, "ptr", ptrType);

}

ResultType(cometCompilerPtr, ErrorMessage) createCompiler(char* inputFilePath, char* sourceCode, bool debugSymbols) {
    CometCompiler* newCompiler = calloc(1, sizeof(CometCompiler));
    if (newCompiler == NULL) {
        ErrorMessage errMsg = createError(
            inputFilePath,
            sourceCode,
            "MemoryAllocFail",
            "newCompiler: failed to allocate memory for CometCompiler struct",
            NULL,
            1,
            1,
            1
        );
        return Error(cometCompilerPtr, ErrorMessage, errMsg);
    }

    newCompiler->stackIdx = 0;
    newCompiler->env = newEnvironment("root", NULL, false);
    newCompiler->structs = newList(cometStructPtr);
    newCompiler->typeMap = newTypemap(NULL);
    newCompiler->libs = newList(charptr);
    newCompiler->currentFunction = NULL;
    newCompiler->inputFilePath = inputFilePath;
    newCompiler->sourceCode = sourceCode;
    newCompiler->currentLine = 0;
    newCompiler->includeDebugSymbols = debugSymbols;
    newCompiler->debugInstInfo = newList(uint64_t);
    newCompiler->cachedGenerics = newList(CachedGenericStruct);
    newCompiler->genericDefinitions = newList(GenericStructDef);
    newCompiler->blocks = newList(Block);

    // fill in type map
    defineType(newCompiler->typeMap, "small",  (CometType){.typeKind = COMET_SMALL });
    defineType(newCompiler->typeMap, "int",    (CometType){.typeKind = COMET_INT   });
    defineType(newCompiler->typeMap, "big",    (CometType){.typeKind = COMET_BIG   });
    defineType(newCompiler->typeMap, "float",  (CometType){.typeKind = COMET_FLOAT });
    defineType(newCompiler->typeMap, "double", (CometType){.typeKind = COMET_DOUBLE});
    defineType(newCompiler->typeMap, "bool",   (CometType){.typeKind = COMET_BOOL  });
    defineType(newCompiler->typeMap, "void",   (CometType){.typeKind = COMET_VOID  });

    // string type
    CometType stringType;
    stringType.typeKind = COMET_ARRAY;

    CometArrayType* stringArrayType = calloc(1, sizeof(CometArrayType));
    stringArrayType->dims = 1;

    CometType* charType = malloc(sizeof(CometType));
    charType->typeKind = COMET_SMALL;

    stringArrayType->elem = charType;
    stringType.arrayType = stringArrayType;

    defineType(newCompiler->typeMap, "string", stringType);

    createExceptionType(newCompiler);
    createPointerType(newCompiler);

    // return new compiler
    
    return Success(cometCompilerPtr, ErrorMessage, newCompiler);
}

ResultType(CometOperand, ErrorMessage) compile(CometCompiler* c, CometASTNode* node) {
    switch (node->nodeType) {
        case AST_PROGRAM:
            return visitProgram(c, node);

        case AST_EXPRESSION_STATEMENT:
            return visitExpressionStatement(c, node);
        case AST_ASSIGN_STATEMENT:
            return visitAssignStatement(c, node);
        case AST_REASSIGN_STATEMENT:
            return visitReassignStatement(c, node);
        case AST_FUNC_DEF_STATEMENT:
            return visitFuncDefStatement(c, node);
        case AST_RETURN_STATEMENT:
            return visitReturnStatement(c, node);
        case AST_IF_STATEMENT:
            return visitIfStatement(c, node);
        case AST_WHILE_STATEMENT:
            return visitWhileStatement(c, node);
        case AST_FOR_STATEMENT:
            return visitForStatement(c, node);
        case AST_STRUCT_DEF_STATEMENT: {
            ResultType(cometTypePtr, ErrorMessage) result = visitStructDefStatement(c, node, false, NULL);
            if (result.error)
                return Error(CometOperand, ErrorMessage, result.as.error);
            return Success(CometOperand, ErrorMessage, NO_OPERAND);
        }
        case AST_NEW_STATEMENT:
            return visitNewStatement(c, node);
        case AST_BREAKPOINT_STATEMENT:
            return visitBreakpointStatement(c);
        case AST_IMPORT_STATEMENT:
            return visitImportStatement(c, node);
        case AST_TRY_STATEMENT:
            return visitTryStatement(c, node);
        case AST_THROW_STATEMENT:
            return visitThrowStatement(c, node);
        
        case AST_FUNC_CALL:
            return visitFuncCall(c, node);
        case AST_INFIX_EXPRESSION: 
            return visitInfixExpression(c, node);
        case AST_PREFIX_EXPRESSION:
            return visitPrefixExpression(c, node);
        
        default: {
            Estr buffer = CREATE_ESTR("No compiler visit method for \"");
            APPEND_ESTR(buffer, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(buffer, "\"!");

            ErrorMessage errMsg = createError(
                c->inputFilePath,
                c->sourceCode,
                "CompilerIssue",
                buffer.str,
                NULL,
                node->lineNum,
                node->startCol,
                node->endCol
            );

            return Error(CometOperand, ErrorMessage, errMsg);
        }
    }
}