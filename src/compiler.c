#include "compiler.h"
#include "ast.h"
#include "inst.h"
#include "lexer.h"
#include "token.h"
#include "parser.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>


// -- HELPER METHODS -- //
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

int32_t getStructIndex(CometCompiler* c, char* structName) {
    for (size_t i = 0; i < c->structs.count; i++) {
        CometStruct* structType = *get(c->structs, i);

        if (strcmp(structType->name, structName) == 0) {
            return i;
        }
    }

    return -1;
}

int32_t getMethodIndex(CometStruct* structType, char* methodName) {
    for (size_t i = 0; i < structType->numMethods; i++) {
        if (strcmp(structType->vtable[i]->name, methodName) == 0) {
            return i;
        }
    }

    return -1;
}

ResultType(CometOperand, charptr) loadExternalLib(CometCompiler* c, const char* path, char* libName) {
    void* handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        Estr errMsg = CREATE_ESTR("Failed to load module: ");
        APPEND_ESTR(errMsg,  dlerror());

        return Error(CometOperand, charptr, errMsg.str);
    }

    void (*onLibImport)(CometEnvironment* env) = dlsym(handle, "onImport");
    if (!onLibImport) {
        Estr errMsg = CREATE_ESTR("Could not get the \"onImport\" function from the library \"");
        APPEND_ESTR(errMsg, libName);
        APPEND_ESTR(errMsg, "\". (");
        APPEND_ESTR(errMsg, dlerror());
        APPEND_ESTR(errMsg, ")");

        dlclose(handle);

        return Error(CometOperand, charptr, errMsg.str);
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
    for (size_t methodIdx = 0; methodIdx < libEnv->recordIdx; methodIdx++) {
        if (libEnv->records[methodIdx].type.typeKind != COMET_FUNCTION) continue;

        Record* libFuncRecord = &libEnv->records[methodIdx];
        CometFunction* funcVal = (CometFunction*)libFuncRecord->value.imm.bigVal; // i sure do love casting pointers to ints lmao

        CometOperand funcOperand = buildFunction(
            c,
            funcVal->name,
            funcVal->argCount,
            funcVal->returnType,
            funcVal->argTypes,
            funcVal->isVarArgs,
            funcVal->isMethod,
            true,
            libIdx
        );

        libEnv->records[methodIdx].value = funcOperand;
    }

    CometOperand libValue = createOperand(CO_IMMEDIATE);
    libValue.imm.typeKind = COMET_MODULE;
    libValue.imm.moduleVal = libEnv;

    CometType libType = {
        .typeKind = COMET_MODULE
    };

    defineVar(oldEnv, libName, RECORD_LOCAL, libValue, libType, false);

    dlclose(handle);
    return Success(CometOperand, charptr, NO_OPERAND);
}

ResultType(CometOperand, charptr) visitProgram(CometCompiler* c, CometASTNode* p) {
    for (size_t i = 0; i < p->data.AST_PROGRAM.numStatements; i++) {
        ResultType(CometOperand, charptr) result = compile(c, p->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;
    }

    return Success(CometOperand, charptr, NO_OPERAND);
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

ResultType(CometOperand, charptr) visitInfixExpression(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, charptr) visitFuncCall(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, charptr) visitNewStatement(CometCompiler* c, CometASTNode* node);
ResultType(CometOperand, charptr) visitValue(CometCompiler* c, CometASTNode* node) {
    switch (node->nodeType) {
        case AST_INFIX_EXPRESSION:
            return visitInfixExpression(c, node);

        case AST_FUNC_CALL:
            return visitFuncCall(c, node);

        case AST_INT: {
            CometOperand new = createOperand(CO_IMMEDIATE);
            new.imm.typeKind = COMET_BIG;
            new.imm.bigVal = node->data.AST_INT.number;

            CometOperand idx = storeConst(c, new);
            buildPushConst(c, idx);

            return Success(CometOperand, charptr, new);
        }

        case AST_BOOL: {
            CometOperand new = createOperand(CO_IMMEDIATE);
            new.imm.typeKind = COMET_BOOL;
            new.imm.boolVal = node->data.AST_BOOL.value;

            CometOperand idx = storeConst(c, new);
            buildPushConst(c, idx);

            return Success(CometOperand, charptr, new);
        }

        case AST_DOUBLE: {
            CometOperand new = createOperand(CO_IMMEDIATE);
            new.imm.typeKind = COMET_DOUBLE;
            new.imm.doubleVal = node->data.AST_DOUBLE.number;

            CometOperand idx = storeConst(c, new);
            buildPushConst(c, idx);

            return Success(CometOperand, charptr, new);
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

            return Success(CometOperand, charptr, new);
        }

        case AST_ARRAY: {
            for (size_t i = 0; i < node->data.AST_ARRAY.elements.count; i++) {
                ResultType(CometOperand, charptr) elementValue = visitValue(c, *get(node->data.AST_ARRAY.elements, i));
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

            return Success(CometOperand, charptr, new);
        }

        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(c->env, varName);

            if (varRecord == NULL) {
                Estr errMsg = CREATE_ESTR("Undefined variable \"");
                APPEND_ESTR(errMsg, varName);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometOperand, charptr, errMsg.str);
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
             
            return Success(CometOperand, charptr, new);
            
        }

        case AST_NEW_STATEMENT: 
            return visitNewStatement(c, node);

        default: {
            Estr errMsg = CREATE_ESTR("Could not build expression: \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\"");

            return Error(CometOperand, charptr, errMsg.str);
        }
    }
}

ResultType(CometOperand, charptr) getModuleValue(CometCompiler* c, CometASTNode* infixExpr);
ResultType(CometType, charptr) resolveType(CometCompiler* c, CometASTNode* node);
ResultType(CometFunctionTypeInfo, charptr) getFunction(CometCompiler* c, CometASTNode* node, bool buildValues) {
    switch (node->nodeType) {
        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(c->env, varName);

            if (varRecord == NULL) {
                Estr errMsg = CREATE_ESTR("Undefined function \"");
                APPEND_ESTR(errMsg, varName);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometFunctionTypeInfo, charptr, errMsg.str);
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

            return Success(CometFunctionTypeInfo, charptr, functionInfo);
        }

        case AST_INFIX_EXPRESSION: {
            struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

            char* fieldName = expr.right->data.AST_IDENTIFIER.ident;

            ResultType(CometType, charptr) structType = resolveType(c, expr.left);
            if (structType.error)
                return Error(CometFunctionTypeInfo, charptr, structType.as.error);

            // get function from module
            if (structType.as.success.typeKind == COMET_MODULE) {
                ResultType(CometOperand, charptr) value = getModuleValue(c, node);
                if (value.error)
                    return Error(CometFunctionTypeInfo, charptr, value.as.error);

                CometFunctionTypeInfo functionInfo = {
                    .funcType = FUNC_FUNC,
                    .value = value.as.success,
                    .methodIdx = (CometOperand){
                        .type = CO_NONE
                    }
                };
                return Success(CometFunctionTypeInfo, charptr, functionInfo);
            }

            if (buildValues) {
                ResultType(CometOperand, charptr) structValue = visitValue(c, expr.left);
                if (structValue.error)
                    return Error(CometFunctionTypeInfo, charptr, structValue.as.error);
            }

            

            int32_t methodIdx = getMethodIndex(structType.as.success.structType, fieldName);

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

            return Success(CometFunctionTypeInfo, charptr, methodInfo);
        }

        default: break;
    }

    return Error(CometFunctionTypeInfo, charptr, "Attempted to call something which isn't a function!");
}

ResultType(astNodeList, charptr) flattenPath(CometCompiler* c, CometASTNode* typeNode) {
    List(astNodePtr) flattenedTypes = newList(astNodePtr);

    switch (typeNode->nodeType) {
        case AST_IDENTIFIER:
            append(flattenedTypes, typeNode);
            break;
        

        case AST_INFIX_EXPRESSION: {
            ResultType(astNodeList, charptr) leftTypes = flattenPath(c, typeNode->data.AST_INFIX_EXPRESSION.left);
            if (leftTypes.error)
                return leftTypes;

            for (size_t i = 0; i < leftTypes.as.success.count; i++) {
                append(flattenedTypes, *get(leftTypes.as.success, i))
            }

            append(flattenedTypes, typeNode->data.AST_INFIX_EXPRESSION.right);

            break;
        }

        default: {
            Estr errMsg = CREATE_ESTR("Expected identifier or module access but got \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(typeNode->nodeType));
            APPEND_ESTR(errMsg, "\"");
            return Error(astNodeList, charptr, errMsg.str);
        }
    }

    return Success(astNodeList, charptr, flattenedTypes);
}

ResultType(CometType, charptr) getTypeByName(CometCompiler* c, char* typeName) {
    List(CometTypeMapEntry) typeMap = c->typeMap;

    for (size_t i = 0; i < typeMap.count; i++) {
        CometTypeMapEntry type = *get(typeMap, i);

        if (strcmp(type.name, typeName) == 0) {
            return Success(CometType, charptr, type.type);
        }
    }

    Estr errMsg = CREATE_ESTR("Cannot find type with name \"");
    APPEND_ESTR(errMsg, typeName);
    APPEND_ESTR(errMsg, "\"");
    return Error(CometType, charptr, errMsg.str);
}

ResultType(CometType, charptr) getType(CometCompiler* c, CometASTNode* typeNode) {
    struct AST_TYPE type = typeNode->data.AST_TYPE;

    // get base type
    CometType* baseType = malloc(sizeof(CometType));

    List(CometTypeMapEntry) typeMap = c->typeMap;
    char* baseTypeName = type.baseType->data.AST_IDENTIFIER.ident;

    bool found = false;
    for (size_t i = 0; i < typeMap.count; i++) {
        CometTypeMapEntry type = *get(typeMap, i);

        if (strcmp(type.name, baseTypeName) == 0) {
            found = true;
            *baseType = type.type;
        }
    }

    if (!found) {
        Estr errMsg = CREATE_ESTR("Unkown type \"");
        APPEND_ESTR(errMsg, baseTypeName);
        APPEND_ESTR(errMsg, "\"");
        return Error(CometType, charptr, errMsg.str);
    }

    CometType finalType;    
    if (type.dimensions > 0) {
        finalType.typeKind = COMET_ARRAY;

        CometArrayType* arrayType = malloc(sizeof(CometArrayType));
        arrayType->elem = baseType;

        if (type.shape.count > MAX_ARRAY_DEPTH) {
            return Error(CometType, charptr, "how deep is that array you're making?????");
        }

        for (size_t i = 0; i < MAX_ARRAY_DEPTH; i++) {
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
        finalType = *baseType;
        free(baseType);
    }

    return Success(CometType, charptr, finalType);
}

ResultType(voidPtr, charptr) visitLValue(CometCompiler* c, CometASTNode* node) {
    switch (node->nodeType) {
        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(c->env, varName);

            if (varRecord == NULL) {
                Estr errMsg = CREATE_ESTR("Undefined variable \"");
                APPEND_ESTR(errMsg, varName);
                APPEND_ESTR(errMsg, "\"");
                return Error(voidPtr, charptr, errMsg.str);
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
  
            return Success(voidPtr, charptr, NULL);
        }

        case AST_INFIX_EXPRESSION: {
            struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

            ResultType(CometType, charptr) leftType = resolveType(c, expr.left);
            if (leftType.error)
                return Error(voidPtr, charptr, leftType.as.error);

            ResultType(CometOperand, charptr) left = visitValue(c, expr.left);
            if (left.error)
                return Error(voidPtr, charptr, left.as.error);


            switch (expr.op.type) {
                case CT_DOT: {
                    if (leftType.as.success.typeKind != COMET_STRUCT) 
                        return Error(voidPtr, charptr, "Attempted to set field of something that isn't a struct!");

                    uint32_t fieldIndex = getFieldIndex(leftType.as.success.structType, expr.right->data.AST_IDENTIFIER.ident);
                    buildGetField(c, fieldIndex);
                    break;
                }

                case CT_COLON: {
                    if (leftType.as.success.typeKind != COMET_ARRAY)
                        return Error(voidPtr, charptr, "Attempted to set element of something that isn't an array!");

                    ResultType(CometOperand, charptr) index = visitValue(c, expr.right);
                    if (index.error)
                        return Error(voidPtr, charptr, index.as.error);
                    
                    break;
                }

                default: {
                    Estr errMsg = CREATE_ESTR("Cannot use operator \"");
                    APPEND_ESTR(errMsg, tokenTypeToCStr(expr.op.type));
                    APPEND_ESTR(errMsg, "\" in lvalue.");
                    return Error(voidPtr, charptr, errMsg.str);
                }
            }

            break;
        }

        default: {
            Estr errMsg = CREATE_ESTR("\"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\" cannot be an lvalue.");
            return Error(voidPtr, charptr, errMsg.str);
        }
    }

    return Success(voidPtr, charptr, NULL);
}

ResultType(CometOperand, charptr) getModuleValue(CometCompiler* c, CometASTNode* infixExpr) {
    struct AST_INFIX_EXPRESSION expr = infixExpr->data.AST_INFIX_EXPRESSION;

    switch (expr.left->nodeType) {
        case AST_IDENTIFIER: {
            char* moduleName = expr.left->data.AST_IDENTIFIER.ident;
            Record* moduleRecord = lookup(c->env, moduleName);
            if (!moduleRecord) {
                Estr errMsg = CREATE_ESTR("Undefined module \"");
                APPEND_ESTR(errMsg, moduleName);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometOperand, charptr, errMsg.str);
            }
            CometOperand module = moduleRecord->value;

            if (module.imm.typeKind != COMET_MODULE)
                return Error(CometOperand, charptr, "Attempted to get an attribute from something that isn't a module!");

            char* attribName = expr.right->data.AST_IDENTIFIER.ident;

            Record* attribRecord = lookup(module.imm.moduleVal, attribName);
            if (!attribRecord) {
                Estr errMsg = CREATE_ESTR("Can't find attribute \"");
                APPEND_ESTR(errMsg, attribName);
                APPEND_ESTR(errMsg, "\" in module \"");
                APPEND_ESTR(errMsg, expr.left->data.AST_IDENTIFIER.ident);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometOperand, charptr, errMsg.str);
            }

            return Success(CometOperand, charptr, attribRecord->value);
        }

        case AST_INFIX_EXPRESSION: {
            char* moduleName = expr.left->data.AST_IDENTIFIER.ident;
            Record* moduleRecord = lookup(c->env, moduleName);
            if (!moduleRecord) {
                Estr errMsg = CREATE_ESTR("Undefined module \"");
                APPEND_ESTR(errMsg, moduleName);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometOperand, charptr, errMsg.str);
            }
            CometOperand module = moduleRecord->value;

            char* attribName = expr.right->data.AST_IDENTIFIER.ident;

            Record* attribRecord = lookup(module.imm.moduleVal, attribName);
            if (!attribRecord) {
                Estr errMsg = CREATE_ESTR("Can't find attribute \"");
                APPEND_ESTR(errMsg, attribName);
                APPEND_ESTR(errMsg, "\" in module \"");
                APPEND_ESTR(errMsg, expr.left->data.AST_IDENTIFIER.ident);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometOperand, charptr, errMsg.str);
            }

            return Success(CometOperand, charptr, attribRecord->value);
        }

        default: {
            return Error(CometOperand, charptr, "getModuleValue: this error should never happen, please make a bug report.");
        }
    }
}

ResultType(CometType, charptr) getModuleAttribType(CometCompiler* c, CometASTNode* node) {
    struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

    if (expr.right->nodeType != AST_IDENTIFIER) {
        Estr errMsg = CREATE_ESTR("Expected attribute name after \".\" but got \"");
        APPEND_ESTR(errMsg, ASTNodeTypeToCStr(expr.right->nodeType));
        APPEND_ESTR(errMsg, "\"");
        return Error(CometType, charptr, errMsg.str);
    }

    switch (expr.left->nodeType) {
        case AST_IDENTIFIER: {
            char* moduleName = expr.left->data.AST_IDENTIFIER.ident;
            Record* moduleRecord = lookup(c->env, moduleName);
            if (!moduleRecord) {
                Estr errMsg = CREATE_ESTR("Undefined module \"");
                APPEND_ESTR(errMsg, moduleName);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometType, charptr, errMsg.str);
            }
            CometOperand module = moduleRecord->value;

            if (module.imm.typeKind != COMET_MODULE)
                return Error(CometType, charptr, "Attempted to get an attribute from something that isn't a module!");

            char* attribName = expr.right->data.AST_IDENTIFIER.ident;

            Record* attribRecord = lookup(module.imm.moduleVal, attribName);
            if (!attribRecord) {
                Estr errMsg = CREATE_ESTR("Can't find attribute \"");
                APPEND_ESTR(errMsg, attribName);
                APPEND_ESTR(errMsg, "\" in module \"");
                APPEND_ESTR(errMsg, expr.left->data.AST_IDENTIFIER.ident);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometType, charptr, errMsg.str);
            }

            return Success(CometType, charptr, attribRecord->type);
        }

        case AST_INFIX_EXPRESSION: {
            ResultType(CometOperand, charptr) left = getModuleValue(c, expr.left);
            if (left.error)
                return Error(CometType, charptr, left.as.error);
            
            char* attribName = expr.right->data.AST_IDENTIFIER.ident;

            Record* attribRecord = lookup(left.as.success.imm.moduleVal, attribName);
            if (!attribRecord) {
                Estr errMsg = CREATE_ESTR("Can't find attribute \"");
                APPEND_ESTR(errMsg, attribName);
                APPEND_ESTR(errMsg, "\" in module \"");
                APPEND_ESTR(errMsg, expr.left->data.AST_IDENTIFIER.ident);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometType, charptr, errMsg.str);
            }

            return Success(CometType, charptr, attribRecord->type);
        }

        
        default: break;
    }

    return Error(CometType, charptr, "Expected identifier or attribute access.");
}

ResultType(CometType, charptr) resolveType(CometCompiler* c, CometASTNode* node) {
    CometValueTypeKind outTypeKind;

    switch (node->nodeType) {
        case AST_INT: outTypeKind = COMET_INT; break;
        case AST_BOOL: outTypeKind = COMET_BOOL; break;
        case AST_DOUBLE: outTypeKind = COMET_DOUBLE; break;

        case AST_STRING: {
            return getTypeByName(c, "string");
        }

        case AST_ARRAY: {
            List(astNodePtr) elements = node->data.AST_ARRAY.elements;

            if (elements.count < 1) {
                return Error(CometType, charptr, "Empty array initializer!");
            }

            CometArrayType* arrayType = malloc(sizeof(CometArrayType));
            // Clear the memory to prevent garbage data in unused depth slots
            memset(arrayType, 0, sizeof(CometArrayType)); 

            // 1. Resolve the very first element to anchor our type layout
            CometASTNode* firstElem = *get(elements, 0);
            ResultType(CometType, charptr) firstResolved = resolveType(c, firstElem);
            if (firstResolved.error) return firstResolved;

            CometType firstType = firstResolved.as.success;

            // 2. Set up the current outermost dimension (Level 0)
            arrayType->isFixedSize[0] = true;
            arrayType->fixedSize[0] = elements.count;

            // 3. Propagate deeper dimensions if the child is already an array
            if (firstType.typeKind == COMET_ARRAY) {
                // Copy the child's dimensions, shifting them down by 1 level
                for (size_t d = 0; d < MAX_ARRAY_DEPTH - 1; d++) {
                    arrayType->isFixedSize[d + 1] = firstType.arrayType->isFixedSize[d];
                    arrayType->fixedSize[d + 1] = firstType.arrayType->fixedSize[d];
                }

                // The immediately nested element type is the child's element type
                arrayType->elem = firstType.arrayType->elem;
            } else {
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
                ResultType(CometType, charptr) siblingResolved = resolveType(c, siblingElem);
                if (siblingResolved.error) return siblingResolved;

                if (!typesAreEqual(firstType, siblingResolved.as.success)) {
                    // Free allocated memory here if needed to avoid leaks!
                    return Error(CometType, charptr, "Inconsistent element types in array literal.");
                }
            }

            CometType outType = {
                .typeKind = COMET_ARRAY,
                .arrayType = arrayType,
            };

            return Success(CometType, charptr, outType);
        }


        case AST_IDENTIFIER: {
            char* varName = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(c->env, varName);

            if (!varRecord) {
                Estr errMsg = CREATE_ESTR("Undefined variable \"");
                APPEND_ESTR(errMsg, varName);
                APPEND_ESTR(errMsg, "\"");
                return Error(CometType, charptr, errMsg.str);
            }

            return Success(CometType, charptr, varRecord->type);
        }
        case AST_NEW_STATEMENT: {
            ResultType(CometType, charptr) type = getType(c, node->data.AST_NEW_STATEMENT.structName);
            if (type.error)
                return type;

            CometType structType = (CometType){
                .typeKind = COMET_STRUCT,
                .structType = type.as.success.structType
            };

            return Success(CometType, charptr, structType);
        }
        case AST_INFIX_EXPRESSION: {
            struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

            ResultType(CometType, charptr) left = resolveType(c, expr.left);
            if (left.error)
                return left;

            switch (expr.op.type) {
                case CT_DIVIDE: // division always results in a double
                    return Success(CometType, charptr, {.typeKind = COMET_DOUBLE});

                case CT_DOT: { // get type of field
                    if (left.as.success.typeKind == COMET_MODULE)
                        return getModuleAttribType(c, node);
                    else if (left.as.success.typeKind != COMET_STRUCT) 
                        return Error(CometType, charptr, "Attempted to get a field of something that isn't a struct!");
                    
                    char* fieldName = expr.right->data.AST_IDENTIFIER.ident;

                    ResultType(CometFunctionTypeInfo, charptr) funcInfo = getFunction(c, expr.right, false);
                    if (!funcInfo.error) {
                        // resolving type of method
                        int32_t methodIdx = getMethodIndex(left.as.success.structType, fieldName);
                        if (methodIdx == -1) {
                            Estr errMsg = CREATE_ESTR("No such method \"");
                            APPEND_ESTR(errMsg, fieldName);
                            APPEND_ESTR(errMsg, "\" in struct \"");
                            APPEND_ESTR(errMsg, left.as.success.structType->name);
                            APPEND_ESTR(errMsg, "\"");

                            return Error(CometType, charptr, errMsg.str);
                        }

                        CometMethod* method = left.as.success.structType->vtable[methodIdx];
                        CometFunction* func = c->functions[method->symbolIdx];

                        CometType funcType = {
                            .typeKind = COMET_FUNCTION,
                            .functionType = func
                        };

                        return Success(CometType, charptr, funcType);
                    }

                    
                    int32_t fieldIdx = getFieldIndex(left.as.success.structType, fieldName);
                    if (fieldIdx == -1) {
                        Estr errMsg = CREATE_ESTR("No such field \"");
                        APPEND_ESTR(errMsg, fieldName);
                        APPEND_ESTR(errMsg, "\" in struct \"");
                        APPEND_ESTR(errMsg, left.as.success.structType->name);
                        APPEND_ESTR(errMsg, "\"");

                        return Error(CometType, charptr, errMsg.str);
                    }

                    CometType fieldType = left.as.success.structType->fieldTypes[fieldIdx];

                    return Success(CometType, charptr, fieldType);
                }
                
                default: break;
            }

            

            ResultType(CometType, charptr) right = resolveType(c, expr.right);

            return Success(CometType, charptr, unifyType(left.as.success, right.as.success));
        }
        case AST_ARG_DEF: {
            return getType(c, node->data.AST_ARG_DEF.type);
        }
        case AST_FUNC_CALL: {
            ResultType(CometFunctionTypeInfo, charptr) funcResult = getFunction(c, node->data.AST_FUNC_CALL.ident, false);
            if (funcResult.error)
                return Error(CometType, charptr, funcResult.as.error);

            CometFunctionTypeInfo funcInfo = funcResult.as.success;
            CometFunction* funcSymbol = c->functions[funcInfo.value.symbolIdx];

            return Success(CometType, charptr, funcSymbol->returnType);
        }

        default: {
            Estr errMsg = CREATE_ESTR("Could not resolve type of expression: \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\"");

            return Error(CometType, charptr, errMsg.str);
        }
    }

    CometType outType = {
        .typeKind = outTypeKind
    };

    return Success(CometType, charptr, outType);
}

// -- VISIT METHODS -- //
ResultType(CometOperand, charptr) visitExpressionStatement(CometCompiler* c, CometASTNode* node) {
    return compile(c, node->data.AST_EXPRESSION_STATEMENT.expression);
}

ResultType(CometOperand, charptr) visitAssignStatement(CometCompiler* c, CometASTNode* node) {
    CometASTNode* expr = node->data.AST_ASSIGN_STATEMENT.expression;
    char* ident = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;

    if (!expr) {
        Estr errMsg = CREATE_ESTR("Variable \"");
        APPEND_ESTR(errMsg, ident);
        APPEND_ESTR(errMsg, "\" was not assigned a value.");
        return Error(CometOperand, charptr, errMsg.str);
    }

    Record* existingVar = lookup(c->env, ident);
    if (existingVar) {
        Estr errMsg = CREATE_ESTR("Redefinition of \"");
        APPEND_ESTR(errMsg, ident);
        APPEND_ESTR(errMsg, "\"")
        return Error(CometOperand, charptr, errMsg.str);
    }

    ResultType(CometType, charptr) exprType = resolveType(c, expr);
    if (exprType.error)
        return Error(CometOperand, charptr, exprType.as.error);

    ResultType(CometType, charptr) varType = getType(c, node->data.AST_ASSIGN_STATEMENT.type);
    if (varType.error)
        return Error(CometOperand, charptr, varType.as.error);
    
    if (!typesAreEqual(varType.as.success, exprType.as.success)) {
        return Error(CometOperand, charptr, "Variable type and expression type don't match in assignment.");
    }

    ResultType(CometOperand, charptr) exprResult = visitValue(c, expr);
    if (exprResult.error)
        return exprResult;

    uint32_t idx = defineVar(c->env, ident, RECORD_LOCAL, exprResult.as.success, exprType.as.success, node->data.AST_ASSIGN_STATEMENT.isMutable);
    buildStore(c, idx);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitFieldReassignStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_INFIX_EXPRESSION expr = node->data.AST_REASSIGN_STATEMENT.ident->data.AST_INFIX_EXPRESSION;

    ResultType(voidPtr, charptr) structResult = visitLValue(c, expr.left);
    if (structResult.error)
        return Error(CometOperand, charptr, structResult.as.error);

    ResultType(CometType, charptr) structType = resolveType(c, expr.left);
    if (structType.error)
        return Error(CometOperand, charptr, structType.as.error);

    if (structType.as.success.typeKind != COMET_STRUCT) {
        return Error(CometOperand, charptr, "Attempted to reassign a field of something that isn't a struct!");
    }

    int32_t fieldIndex = getFieldIndex(structType.as.success.structType, expr.right->data.AST_IDENTIFIER.ident);
    ResultType(CometType, charptr) exprType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.expression);
    if (exprType.error)
        return Error(CometOperand, charptr, exprType.as.error);

    ResultType(CometType, charptr) varType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.ident);
    if (varType.error) 
        return Error(CometOperand, charptr, varType.as.error);
    
    if (node->data.AST_REASSIGN_STATEMENT.op.type != CT_EQ) {
        buildGetField(c, fieldIndex);
    }

    CometType resultType = unifyType(varType.as.success, exprType.as.success);
    if (resultType.typeKind != varType.as.success.typeKind) {
        Estr errMsg = CREATE_ESTR("Attempted to reassign type of field in struct \"");
        APPEND_ESTR(errMsg, structType.as.success.structType->name);
        APPEND_ESTR(errMsg, "\" at runtime!");
        return Error(CometOperand, charptr, errMsg.str);
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
            

            Estr errMsg = CREATE_ESTR("Cannot use operator \"");
            APPEND_ESTR(errMsg, tokenTypeToCStr(expr.op.type));
            APPEND_ESTR(errMsg, "\" to reassign field of struct \"");
            APPEND_ESTR(errMsg, structType.as.success.structType->name);
            APPEND_ESTR(errMsg, "\".");
            return Error(CometOperand, charptr, errMsg.str);
        }
    }

    if (node->data.AST_REASSIGN_STATEMENT.op.type != CT_EQ) {
        visitLValue(c, expr.left);
    }

    buildSetField(c, fieldIndex);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitArrayReassignStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_INFIX_EXPRESSION expr = node->data.AST_REASSIGN_STATEMENT.ident->data.AST_INFIX_EXPRESSION;

    ResultType(voidPtr, charptr) arrayResult = visitLValue(c, node->data.AST_REASSIGN_STATEMENT.ident);
    if (arrayResult.error)
        return Error(CometOperand, charptr, arrayResult.as.error);

    ResultType(CometType, charptr) arrayType = resolveType(c, expr.left);
    if (arrayType.error)
        return Error(CometOperand, charptr, arrayType.as.error);

    if (arrayType.as.success.typeKind != COMET_ARRAY) {
        return Error(CometOperand, charptr, "Attempted to reassign an element of something that isn't an array!");
    }

    ResultType(CometType, charptr) exprType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.expression);
    if (exprType.error)
        return Error(CometOperand, charptr, exprType.as.error);

    ResultType(CometType, charptr) varType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.ident);
    if (varType.error) 
        return Error(CometOperand, charptr, varType.as.error);
    
    if (node->data.AST_REASSIGN_STATEMENT.op.type != CT_EQ) {
        buildListAt(c);
    }

    CometType resultType = unifyType(varType.as.success, exprType.as.success);
    if (resultType.typeKind != varType.as.success.typeKind) {
        return Error(CometOperand, charptr, "Attempted to reassign type of element in array at runtime!");
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
            

            Estr errMsg = CREATE_ESTR("Cannot use operator \"");
            APPEND_ESTR(errMsg, tokenTypeToCStr(expr.op.type));
            APPEND_ESTR(errMsg, "\" to reassign element of array!");
            return Error(CometOperand, charptr, errMsg.str);
        }
    }

    if (node->data.AST_REASSIGN_STATEMENT.op.type != CT_EQ) {
        visitLValue(c, expr.left);
    }

    buildListSet(c);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitReassignStatement(CometCompiler* c, CometASTNode* node) {
    ResultType(CometOperand, charptr) exprResult = visitValue(c, node->data.AST_ASSIGN_STATEMENT.expression);
    if (exprResult.error)
        return exprResult;

    if (node->data.AST_REASSIGN_STATEMENT.ident->nodeType == AST_INFIX_EXPRESSION) { // infix reassign

        struct AST_INFIX_EXPRESSION leftExpr = node->data.AST_REASSIGN_STATEMENT.ident->data.AST_INFIX_EXPRESSION;

        if (leftExpr.op.type == CT_DOT) { // struct reassign
            return visitFieldReassignStatement(c, node);
        } else if (leftExpr.op.type == CT_COLON) {
            return visitArrayReassignStatement(c, node);
        } else {
            Estr errMsg = CREATE_ESTR("Cannot use operator \"");
            APPEND_ESTR(errMsg, tokenTypeToCStr(leftExpr.op.type));
            APPEND_ESTR(errMsg, "\" in reassignment.");
            return Error(CometOperand, charptr, errMsg.str);
        }
        
    }

    ResultType(CometType, charptr) varType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.ident);
    if (varType.error)
        return Error(CometOperand, charptr, varType.as.error);

    char* ident = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;

    Record* varRecord = lookup(c->env, ident);
    if (!varRecord) {
        Estr errMsg = CREATE_ESTR("Cannot reassign undefined variable \"");
        APPEND_ESTR(errMsg, ident);
        APPEND_ESTR(errMsg, "\"");
        return Error(CometOperand, charptr, errMsg.str);
    }

    if (!varRecord->isMutable) {
        Estr errMsg = CREATE_ESTR("Cannot change value of immutable variable \"");
        APPEND_ESTR(errMsg, ident);
        APPEND_ESTR(errMsg, "\", did you forget \"mut\"?");
        return Error(CometOperand, charptr, errMsg.str);
    }

    if (node->data.AST_REASSIGN_STATEMENT.op.type != CT_EQ) {
        buildLoad(c, varRecord->recordIdx);
    }

    ResultType(CometType, charptr) exprType = resolveType(c, node->data.AST_REASSIGN_STATEMENT.expression);
    if (exprType.error)
        return Error(CometOperand, charptr, exprType.as.error);

    CometType resultType = unifyType(varType.as.success, exprType.as.success);
    if (resultType.typeKind != varType.as.success.typeKind) {
        Estr errMsg = CREATE_ESTR("Attempted to reassign type of variable \"");
        APPEND_ESTR(errMsg, ident);
        APPEND_ESTR(errMsg, "\" at runtime!");
        return Error(CometOperand, charptr, errMsg.str);
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
            

            Estr errMsg = CREATE_ESTR("Cannot use operator \"");
            APPEND_ESTR(errMsg, tokenTypeToCStr(node->data.AST_REASSIGN_STATEMENT.op.type));
            APPEND_ESTR(errMsg, "\" to reassign varialbe \"");
            APPEND_ESTR(errMsg, ident);
            APPEND_ESTR(errMsg, "\".");
            return Error(CometOperand, charptr, errMsg.str);
        }
    }

    buildStore(c, varRecord->recordIdx);
    return Success(CometOperand, charptr, NO_OPERAND);
}

ResultType(CometOperand, charptr) getField(CometCompiler* c, CometASTNode* structToGet, CometASTNode* field) {
    char* fieldName = field->data.AST_IDENTIFIER.ident;

    ResultType(CometType, charptr) structType = resolveType(c, structToGet);
    if (structType.error)
        return Error(CometOperand, charptr, structType.as.error);

    ResultType(CometOperand, charptr) structValue = visitValue(c, structToGet);
    if (structValue.error) 
        return structValue;

    int32_t fieldIdx = getFieldIndex(structType.as.success.structType, fieldName);
    CometOperand dest = buildGetField(c, fieldIdx);

    return Success(CometOperand, charptr, dest);
}
ResultType(CometOperand, charptr) visitInfixExpression(CometCompiler* c, CometASTNode* node) {
    struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

    ResultType(CometType, charptr) leftType = resolveType(c, expr.left);
    if (leftType.error)
        return Error(CometOperand, charptr, leftType.as.error);

    if (expr.op.type == CT_DOT) { // getting a field
        if (leftType.as.success.typeKind == COMET_MODULE)
            return getModuleValue(c, node);

        return getField(c, expr.left, expr.right);
    }

    ResultType(CometType, charptr) rightType = resolveType(c, expr.right);
    if (rightType.error)
        return Error(CometOperand, charptr, rightType.as.error);

    CometType resultType = unifyType(leftType.as.success, rightType.as.success);
    
    // left
    ResultType(CometOperand, charptr) leftValue = visitValue(c, expr.left);
    if (leftValue.error)
        return leftValue;

    if (typesAreEqual(leftType.as.success, resultType)) {
        leftType.as.success = buildCast(c, leftType.as.success, resultType);
    }

    // right
    ResultType(CometOperand, charptr) rightValue = visitValue(c, expr.right);
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
            Estr errMsg = CREATE_ESTR("Invalid operator for int and int: \"");
            APPEND_ESTR(errMsg, tokenTypeToCStr(expr.op.type));
            APPEND_ESTR(errMsg, "\"");

            return Error(CometOperand, charptr, errMsg.str);
        }
    }

    return Success(CometOperand, charptr, out);
}
ResultType(CometOperand, charptr) visitFuncDefStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_FUNC_DEF_STATEMENT funcDef = node->data.AST_FUNC_DEF_STATEMENT;
    char* funcName = funcDef.ident->data.AST_IDENTIFIER.ident;

    if (node->data.AST_FUNC_DEF_STATEMENT.args.count > MAX_ARGS) {
        char* buffer = malloc(64);
        snprintf(buffer, 64, "Functions can't have more than %d args.", MAX_ARGS);
        return Error(CometOperand, charptr, buffer);
    }

    // get arg types
    CometType* argTypes = calloc(funcDef.args.count, sizeof(CometType));
    for (size_t argTypeIdx = 0; argTypeIdx < funcDef.args.count; argTypeIdx++) {
        CometASTNode* argNode = *get(funcDef.args, argTypeIdx);

        ResultType(CometType, charptr) argType = getType(c, argNode->data.AST_ARG_DEF.type);
        if (argType.error)
            return Error(CometOperand, charptr, argType.as.error);

        argTypes[argTypeIdx] = argType.as.success;
    } 

    // get return type
    ResultType(CometType, charptr) returnType = getType(c, funcDef.returnType);
    if (returnType.error)
        return Error(CometOperand, charptr, returnType.as.error);

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

        ResultType(CometType, charptr) argType = resolveType(c, argNode);
        if (argType.error)
            return Error(CometOperand, charptr, argType.as.error);

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
    ResultType(CometOperand, charptr) bodyResult = compile(c, funcDef.program);
    if (bodyResult.error)
        return bodyResult;

    // return back to the parent scope
    c->env = destroyEnv(c->env);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitReturnStatement(CometCompiler* c, CometASTNode* node) {
    if (c->currentFunction->returnType.typeKind != COMET_VOID) {
        ResultType(CometOperand, charptr) returnValue = visitValue(c, node->data.AST_RETURN_STATEMENT.expression);
        if (returnValue.error)
            return returnValue;
    }
    
    buildReturn(c);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitFuncCall(CometCompiler* c, CometASTNode* node) {
    struct AST_FUNC_CALL funcCall = node->data.AST_FUNC_CALL;

    ResultType(CometType, charptr) funcParentType = resolveType(c, funcCall.ident);
    if (funcParentType.error)
        return Error(CometOperand, charptr, funcParentType.as.error);

    ResultType(CometFunctionTypeInfo, charptr) funcVal = getFunction(c, funcCall.ident, false);
    if (funcVal.error)
        return Error(CometOperand, charptr, funcVal.as.error);

    CometFunction* func = c->functions[funcVal.as.success.value.symbolIdx];
    uint32_t neededArgCount = func->isMethod ? func->argCount - 1 : func->argCount;

    if (funcCall.args.count < neededArgCount) {
        Estr errMsg = CREATE_ESTR("Not enough args passed to function \"");
        APPEND_ESTR(errMsg, func->name);
        APPEND_ESTR(errMsg, "\"");
        return Error(CometOperand, charptr, errMsg.str);
    } else if (funcCall.args.count > neededArgCount && !func->isVarArgs) {
        Estr errMsg = CREATE_ESTR("Too many args passed to function \"");
        APPEND_ESTR(errMsg, func->name);
        APPEND_ESTR(errMsg, "\"");
        return Error(CometOperand, charptr, errMsg.str);
    }

    List(CometOperand) funcCallArgs = newList(CometOperand);
    for (size_t argIdx = 0; argIdx < funcCall.args.count; argIdx++) {
        CometASTNode* argNode = *get(funcCall.args, argIdx);

        ResultType(CometOperand, charptr) argValue = visitValue(c, argNode);
        if (argValue.error)
            return argValue;

        append(funcCallArgs, argValue.as.success);
    }

    funcVal = getFunction(c, funcCall.ident, true);

    CometOperand returnValue;
    if (funcVal.as.success.funcType == FUNC_FUNC) {
        returnValue = buildCall(c, func->name, funcCallArgs);
    } else { // FUNC_METHOD
        returnValue = buildCallMethod(c, funcVal.as.success.methodIdx.imm.smallVal, funcCallArgs);
    }

    return Success(CometOperand, charptr, returnValue);
}
ResultType(CometOperand, charptr) visitIfStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_IF_STATEMENT ifStmt = node->data.AST_IF_STATEMENT;
    CometASTNode* elseBody = ifStmt.elseProgram;

    CometLabel* endLabel = buildLabel(c);
    CometLabel* elseLabel = buildLabel(c);

    ResultType(CometOperand, charptr) condition = visitValue(c, ifStmt.expression);
    if (condition.error)
        return condition;

    if (elseBody != NULL)
        buildJumpIfFalse(c, elseLabel);
    else
        buildJumpIfFalse(c, endLabel);

    ResultType(CometOperand, charptr) ifBodyResult = compile(c, ifStmt.program);
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

        ResultType(CometOperand, charptr) elseBodyResult = compile(c, ifStmt.elseProgram);
        if (elseBodyResult.error)
            return elseBodyResult;

    }

    resolveLabel(c, endLabel);
    

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitWhileStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_WHILE_STATEMENT whileStmt = node->data.AST_WHILE_STATEMENT;
    
    CometLabel* startLabel = buildLabel(c);
    CometLabel* endLabel = buildLabel(c);

    CometEnvironment* whileEnv = newEnvironment("whileLoop", c->env, false);
    c->env = whileEnv;

    ResultType(CometOperand, charptr) condition = visitValue(c, whileStmt.expression);
    if (condition.error)
        return condition;

    buildJumpIfFalse(c, endLabel);
    resolveLabel(c, startLabel);

    ResultType(CometOperand, charptr) whileBodyResult = compile(c, whileStmt.program);
    if (whileBodyResult.error)
        return whileBodyResult;

    condition = visitValue(c, whileStmt.expression);
    if (condition.error)
        return condition;

    buildJumpIfTrue(c, startLabel);
    resolveLabel(c, endLabel);

    c->env = destroyEnv(whileEnv);

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitForStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_FOR_STATEMENT forStmt = node->data.AST_FOR_STATEMENT;

    CometLabel* mainLabel = buildLabel(c);
    CometLabel* endLabel = buildLabel(c);

    // resolve start and end types
    ResultType(CometType, charptr) startType = resolveType(c, forStmt.start);
    if (startType.error)
        return Error(CometOperand, charptr, startType.as.error);
    ResultType(CometType, charptr) endType = resolveType(c, forStmt.end);
    if (endType.error)
        return Error(CometOperand, charptr, endType.as.error);
    CometType resultType = unifyType(startType.as.success, endType.as.success);

    char* ident = forStmt.ident->data.AST_IDENTIFIER.ident;

    // create env for for loop
    CometEnvironment* forLoopEnv = newEnvironment("forLoop", c->env, false);
    c->env = forLoopEnv;

    // define iterator variable
    ResultType(CometOperand, charptr) start = visitValue(c, forStmt.start);
    if (start.error)
        return Error(CometOperand, charptr, start.as.error);

    uint32_t idx = defineVar(c->env, ident, RECORD_LOCAL, start.as.success, resultType, false);
    buildStore(c, idx);

    resolveLabel(c, mainLabel);

    // if the iterator var is equal to the end value, then we jump to the exit of the for loop
    buildLoad(c, idx);

    ResultType(CometOperand, charptr) end  = visitValue(c, forStmt.end);
    if (end.error)
        return Error(CometOperand, charptr, end.as.error);

    buildNeq(c, startType.as.success);
    buildJumpIfFalse(c, endLabel);

    // compile the body of the for loop
    ResultType(CometOperand, charptr) bodyResult = compile(c, forStmt.program);
    if (bodyResult.error)
        return bodyResult;

    // compile the step value
    ResultType(CometType, charptr) stepType = resolveType(c, forStmt.step);
    if (stepType.error)
        return Error(CometOperand, charptr, stepType.as.error);

    buildLoad(c, idx);

    ResultType(CometOperand, charptr) step = visitValue(c, forStmt.step);
    if (step.error)
        return Error(CometOperand, charptr, step.as.error);

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

    

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitConstructorDefStatement(CometCompiler* c, CometASTNode* node, char* constructorName, CometType structType, CometStruct* parentStruct) {
    struct AST_CONSTRUCTOR_DEF constDef = node->data.AST_CONSTRUCTOR_DEF;

    // get arg types
    CometType* argTypes = calloc(constDef.args.count + 1, sizeof(CometType));
    for (size_t argTypeIdx = 0; argTypeIdx < constDef.args.count; argTypeIdx++) {
        CometASTNode* argNode = *get(constDef.args, argTypeIdx);

        ResultType(CometType, charptr) argType = getType(c, argNode);
        if (argType.error)
            return Error(CometOperand, charptr, argType.as.error);

        argTypes[argTypeIdx+1] = argType.as.success;
    }

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

        ResultType(CometType, charptr) argType = resolveType(c, argNode);
        if (argType.error)
            return Error(CometOperand, charptr, argType.as.error);

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
    ResultType(CometOperand, charptr) bodyResult = compile(c, constDef.program);
    if (bodyResult.error)
        return bodyResult;

    // build return
    buildLoadArg(c, selfIdx);
    buildReturn(c);

    // return back to the parent scope
    c->env = destroyEnv(funcEnv);
    
    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitMethodDefStatement(CometCompiler* c, CometASTNode* node, CometType structType) {
    struct AST_FUNC_DEF_STATEMENT funcDef = node->data.AST_FUNC_DEF_STATEMENT;
    char* funcName = funcDef.ident->data.AST_IDENTIFIER.ident;

    // get arg types
    CometType* argTypes = calloc(funcDef.args.count+1, sizeof(CometType));
    for (size_t argTypeIdx = 0; argTypeIdx < funcDef.args.count; argTypeIdx++) {
        CometASTNode* argNode = *get(funcDef.args, argTypeIdx);

        ResultType(CometType, charptr) argType = getType(c, argNode);
        if (argType.error)
            return Error(CometOperand, charptr, argType.as.error);

        argTypes[argTypeIdx+1] = argType.as.success;
    } 

    // get return type
    ResultType(CometType, charptr) returnType = getType(c, funcDef.returnType);
    if (returnType.error)
        return Error(CometOperand, charptr, returnType.as.error);

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

        ResultType(CometType, charptr) argType = resolveType(c, argNode);
        if (argType.error)
            return Error(CometOperand, charptr, argType.as.error);

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
    ResultType(CometOperand, charptr) bodyResult = compile(c, funcDef.program);
    if (bodyResult.error)
        return bodyResult;

    // return back to the parent scope
    c->env = destroyEnv(c->env);

    return Success(CometOperand, charptr, funcValue);
}
ResultType(CometOperand, charptr) visitStructDefStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_STRUCT_DEF_STATEMENT structDef = node->data.AST_STRUCT_DEF_STATEMENT;

    CometStruct* structType = malloc(sizeof(CometStruct));
    char* structName = structDef.ident->data.AST_IDENTIFIER.ident;

    structType->name = structName;

    

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
        ResultType(CometType, charptr) parentStructType = getType(c, structDef.parentName);
        if (parentStructType.error)
            return Error(CometOperand, charptr, parentStructType.as.error);

        if (parentStructType.as.success.typeKind != COMET_STRUCT) {
            Estr errMsg = CREATE_ESTR("Cannot inherit from non-struct type \"");
            APPEND_ESTR(errMsg, structDef.parentName->data.AST_IDENTIFIER.ident);
            APPEND_ESTR(errMsg, "\"");
            return Error(CometOperand, charptr, errMsg.str);
        }

        parentStruct = parentStructType.as.success.structType;

        // add parent field count
        parentFieldCount = parentStruct->fieldCount;
        parentMethodCount = parentStruct->numMethods;
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
                Estr errMsg = CREATE_ESTR("Cannot define \"");
                APPEND_ESTR(errMsg, ASTNodeTypeToCStr(fieldDef->nodeType));
                APPEND_ESTR(errMsg, "\" in struct.");

                return Error(CometOperand, charptr, errMsg.str);
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

    CometTypeMapEntry typeMapEntry = {
        .name = strdup(structName),
        .type = {
            .typeKind = COMET_STRUCT,
            .structType = structType
        }
    };

    append(c->typeMap, typeMapEntry);
    append(c->structs, structType);

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
                ResultType(CometType, charptr) fieldType = getType(c, fieldDef->data.AST_ASSIGN_STATEMENT.type);
                if (fieldType.error)
                    return Error(CometOperand, charptr, fieldType.as.error);

                structType->fieldNames[fieldIdx] = fieldDef->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;
                structType->fieldTypes[fieldIdx++] = fieldType.as.success;
                break;
            }

            case AST_OVERRIDE_STATEMENT: {
                // we're not inheriting from any struct so we cant override functions
                if (parentStruct == NULL) {
                    Estr errMsg = CREATE_ESTR("Cannot use an override statement in struct \"");
                    APPEND_ESTR(errMsg, structName);
                    APPEND_ESTR(errMsg, "\" because it has no parent.");
                    return Error(CometOperand, charptr, errMsg.str);
                }

                ResultType(CometOperand, charptr) result = visitMethodDefStatement(c, fieldDef->data.AST_OVERRIDE_STATEMENT.funcDef, generalStructType);
                if (result.error)
                    return result;

                CometFunction* function = c->functions[result.as.success.symbolIdx];
                int32_t parentMethodIdx = getMethodIndex(parentStruct, function->name);
            
                // overriding a function that doesn't exist in the parent
                if (parentMethodIdx == -1) {
                    Estr errMsg = CREATE_ESTR("Cannot override method \"");
                    APPEND_ESTR(errMsg, function->name);
                    APPEND_ESTR(errMsg, "\" because parent struct doesn't have it.");
                    return Error(CometOperand, charptr, errMsg.str);
                }

                Estr newFuncName = CREATE_ESTR(structName);
                APPEND_ESTR(newFuncName, "_");
                APPEND_ESTR(newFuncName, function->name);

                CometMethod* newMethod = malloc(sizeof(CometMethod));
                memcpy(newMethod->name, function->name, strlen(function->name) + 1);
                memcpy(function->name, newFuncName.str, newFuncName.size + 1);
                newMethod->argCount = function->argCount;
                newMethod->startIdx = function->startIdx,
                newMethod->symbolIdx = result.as.success.symbolIdx;

                DESTROY_ESTR(newFuncName);

                structType->vtable[parentMethodIdx] = newMethod;
                break;
            }

            case AST_FUNC_DEF_STATEMENT: {

                ResultType(CometOperand, charptr) result = visitMethodDefStatement(c, fieldDef, generalStructType);
                if (result.error)
                    return result;

                CometFunction* function = c->functions[result.as.success.symbolIdx];

                Estr newFuncName = CREATE_ESTR(structName);
                APPEND_ESTR(newFuncName, "_");
                APPEND_ESTR(newFuncName, function->name);

                CometMethod* newMethod = malloc(sizeof(CometMethod));
                memcpy(newMethod->name, function->name, strlen(function->name) + 1);
                memcpy(function->name, newFuncName.str, newFuncName.size + 1);
                newMethod->argCount = function->argCount;
                newMethod->startIdx = function->startIdx,
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
        Estr errMsg = CREATE_ESTR("Struct \"");
        APPEND_ESTR(errMsg, structName);
        APPEND_ESTR(errMsg, "\" is missing a constructor!");

        return Error(CometOperand, charptr, errMsg.str);
    }
    Estr constructorName = CREATE_ESTR(strdup(structName));
    APPEND_ESTR(constructorName, "_INIT");

    ResultType(CometOperand, charptr) constructorResult = visitConstructorDefStatement(c, structDef.constructor, constructorName.str, generalStructType, parentStruct);
    if (constructorResult.error)
        return constructorResult;

    

    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitNewStatement(CometCompiler* c, CometASTNode* node) {
    struct AST_NEW_STATEMENT newStmt = node->data.AST_NEW_STATEMENT;

    // get struct type
    char* structName = newStmt.structName->data.AST_IDENTIFIER.ident;
    int32_t idx = getStructIndex(c, structName);

    if (idx == -1) {
        Estr errMsg = CREATE_ESTR("The type \"");
        APPEND_ESTR(errMsg, structName);
        APPEND_ESTR(errMsg, "\" was not found.");

        return Error(CometOperand, charptr, errMsg.str);
    }

    // make new instance
    buildNew(c, idx);

    // push args for constructor
    List(CometOperand) funcCallArgs = newList(CometOperand);
    for (size_t argIdx = 0; argIdx < newStmt.args.count; argIdx++) {
        CometASTNode* argNode = *get(newStmt.args, argIdx);

        ResultType(CometOperand, charptr) argValue = visitValue(c, argNode);
        if (argValue.error)
            return argValue;

        append(funcCallArgs, argValue.as.success);
    }

    // call constructor
    Estr constructorName = CREATE_ESTR(structName);
    APPEND_ESTR(constructorName, "_INIT");
    buildCall(c, constructorName.str, funcCallArgs);
    DESTROY_ESTR(constructorName);

    // return
    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitBreakpointStatement(CometCompiler* c) {
    buildBreakpoint(c);
    return Success(CometOperand, charptr, NO_OPERAND);
}
ResultType(CometOperand, charptr) visitImportStatement(CometCompiler* c, CometASTNode* node) {
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

    char* cometLibsPath = getenv("COMET_LIBS");
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
        Estr errMsg = CREATE_ESTR("Could not find library \"");
        APPEND_ESTR(errMsg, libName);
        APPEND_ESTR(errMsg, "\"");
        return Error(CometOperand, charptr, errMsg.str);
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
    ResultType(CometLexer, charptr) lexer = newLexer(fileContents);
    if (lexer.error) {
        APPEND_ESTR(errMsg, lexer.as.error);
        return Error(CometOperand, charptr, errMsg.str);
    }
    ResultType(tokenList, charptr) tokens = lex(&lexer.as.success);
    if (tokens.error) {
        APPEND_ESTR(errMsg, tokens.as.error);
        return Error(CometOperand, charptr, errMsg.str);
    }
    free(fileContents);

    // parse
    ResultType(parserPtr, charptr) parser = newParser(tokens.as.success);
    if (parser.error) {
        APPEND_ESTR(errMsg, parser.as.error);
        return Error(CometOperand, charptr, errMsg.str);
    }
    ResultType(astNodePtr, charptr) ast = buildAST(parser.as.success);
    if (ast.error) {
        APPEND_ESTR(errMsg, ast.as.error);
        return Error(CometOperand, charptr, errMsg.str);
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

    ResultType(CometOperand, charptr) importedCompileResult = compile(c, ast.as.success);
    if (importedCompileResult.error) {
        APPEND_ESTR(errMsg, importedCompileResult.as.error);
        return Error(CometOperand, charptr, errMsg.str);
    }

    defineVar(prevEnv, lastModuleIdent, RECORD_LOCAL, module, moduleType, false);

    freeNode(ast.as.success);
    free(parser.as.success);

    c->env = prevEnv;

    return Success(CometOperand, charptr, NO_OPERAND);
}

// -- MAIN -- //
ResultType(voidPtr, charptr) outputToFile(CometCompiler* c, const char* filePath) {
    FILE* file = fopen(filePath, "wb");
    if (file == NULL) {
        return Error(voidPtr, charptr, strerror(errno));
    }

    CometFile cometFile = {
        .magic = {'C', 'O', 'M', 'E',  'T'},
        .version = 1,
        .numConsts = c->constIdx,
        .numInstructions = c->programIdx,
        .numFunctions = c->functionCount,
        .numStructs = c->structs.count,
        .numLibs = c->libs.count
    };

    fwrite(&cometFile, sizeof(CometFile), 1, file);
    fwrite(c->consts, sizeof(CometOperand), c->constIdx, file);


    for (size_t i = 0; i < c->functionCount; i++) {
        

        CometSerializedFunc serializedFunc = {
            .startIdx = c->functions[i]->startIdx,
            .numArgs = c->functions[i]->argCount,
            .isExternal = c->functions[i]->isExternal,
            .libIdx = c->functions[i]->libIdx,
            .isVarArgs = c->functions[i]->isVarArgs
        };
        strcpy(serializedFunc.name, c->functions[i]->name);
        memcpy(serializedFunc.argTypes, c->functions[i]->argTypes, sizeof(CometType) * c->functions[i]->argCount);

        fwrite(&serializedFunc, sizeof(CometSerializedFunc), 1, file);
    }

    for (size_t structIdx = 0; structIdx < c->structs.count; structIdx++) {
        CometStruct* structType = *get(c->structs, structIdx);
        CometSerializedStruct* serializedStruct = serializeStruct(structType);

        CometSerializedStructHeader header = {
            .numFields = serializedStruct->numFields,
            .numMethods = serializedStruct->numMethods
        };

        fwrite(&header, sizeof(CometSerializedStructHeader), 1, file);
        fwrite(serializedStruct->vtable, sizeof(uint32_t), serializedStruct->numMethods, file);
    }

    for (size_t libIdx = 0; libIdx < c->libs.count; libIdx++) {
        char* libName = *get(c->libs, libIdx);
        fwrite(libName, 64, 1, file);
    }

    for (size_t instIdx = 0; instIdx < c->programIdx; instIdx++) {
        CometInst inst = c->outputProgram[instIdx];
        CometSerializedInst* serializedInst = serializeInst(inst);

        fwrite(serializedInst, sizeof(CometSerializedInst), 1, file);
        
    }

    fclose(file);

    return Success(voidPtr, charptr, NULL);
}

ResultType(cometCompilerPtr, charptr) newCompiler() {
    CometCompiler* newCompiler = calloc(1, sizeof(CometCompiler));

    newCompiler->outputProgram = calloc(pow(2, 14), sizeof(CometInst));
    newCompiler->programIdx = 0;
    newCompiler->stackIdx = 0;
    newCompiler->env = newEnvironment("root", NULL, false);
    newCompiler->structs = newList(cometStructPtr);
    newCompiler->typeMap = newList(CometTypeMapEntry);
    newCompiler->libs = newList(charptr);
    newCompiler->currentFunction = NULL;
 
    // fill in type map
    CometTypeMapEntry smallType =  { .name = "small",  .type = (CometType){.typeKind = COMET_SMALL}  };
    CometTypeMapEntry intType =    { .name = "int",    .type = (CometType){.typeKind = COMET_INT}    };
    CometTypeMapEntry bigType =    { .name = "big",    .type = (CometType){.typeKind = COMET_BIG}    };
    CometTypeMapEntry boolType =   { .name = "bool",   .type = (CometType){.typeKind = COMET_BOOL}   };
    CometTypeMapEntry floatType =  { .name = "float",  .type = (CometType){.typeKind = COMET_FLOAT}  };
    CometTypeMapEntry doubleType = { .name = "double", .type = (CometType){.typeKind = COMET_DOUBLE} };
    CometTypeMapEntry voidType = { .name = "void", .type = (CometType){.typeKind = COMET_VOID} };
    append(newCompiler->typeMap, smallType);
    append(newCompiler->typeMap, intType);
    append(newCompiler->typeMap, bigType);
    append(newCompiler->typeMap, boolType);
    append(newCompiler->typeMap, floatType);
    append(newCompiler->typeMap, doubleType);
    append(newCompiler->typeMap, voidType);

    // string type
    CometType stringType;
    stringType.typeKind = COMET_ARRAY;

    CometArrayType* stringArrayType = calloc(1, sizeof(CometArrayType));

    CometType* charType = malloc(sizeof(CometType));
    charType->typeKind = COMET_SMALL;

    stringArrayType->elem = charType;
    stringType.arrayType = stringArrayType;

    CometTypeMapEntry stringTypeEntry = { .name = "string", .type = stringType};
    append(newCompiler->typeMap, stringTypeEntry);


    // return new compiler
    
    return Success(cometCompilerPtr, charptr, newCompiler);
}

ResultType(CometOperand, charptr) compile(CometCompiler* c, CometASTNode* node) {
    
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
        case AST_STRUCT_DEF_STATEMENT:
            return visitStructDefStatement(c, node);
        case AST_NEW_STATEMENT:
            return visitNewStatement(c, node);
        case AST_BREAKPOINT_STATEMENT:
            return visitBreakpointStatement(c);
        case AST_IMPORT_STATEMENT:
            return visitImportStatement(c, node);
        
        case AST_FUNC_CALL:
            return visitFuncCall(c, node);
        case AST_INFIX_EXPRESSION: 
            return visitInfixExpression(c, node);
        
        default: {
            Estr errMsg = CREATE_ESTR("No compiler visit method for \"");
            APPEND_ESTR(errMsg, ASTNodeTypeToCStr(node->nodeType));
            APPEND_ESTR(errMsg, "\"!");

            return Error(CometOperand, charptr, errMsg.str);
        }
    }
}