#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "../include/comet_operand.h"
#include "../include/util.h"
#include "../include/error_message.h"
#include "../lib/estr.h"
#include "args.h"
#include "debugger.h"

typedef void* voidPtr;
Result(voidPtr, charptr);

// For Clang and GCC on macOS
#define FORCE_INLINE __attribute__((always_inline)) static inline

void createBreakpoint(CometVM* vm) {
    vm->breakpoints[vm->currentFrame->ip - 1] = 1;
}

FORCE_INLINE void pushValue(CometVM* vm, int64_t value) {
    vm->stack[vm->sp++] = value;
}

FORCE_INLINE int64_t getTop(CometVM* vm) {
    if (vm->sp <= 0) {
        fprintf(stderr, "Attempted to popValue top of stack while stack was empty, this is a compiler bug! Please report this at https://chookspace.com/Comet/Comet/issues with your code.\n");
        fprintf(stderr, "%s\n", stackTrace(vm));
        exit(1);
    }

    return vm->stack[vm->sp-1];
}

FORCE_INLINE int64_t popValue(CometVM* vm) {
    int64_t value = getTop(vm);
    vm->sp--;
    return value;
}

FORCE_INLINE void pushImm(CometVM* vm, CometImmediate imm) {
    switch (imm.typeKind) {
        case COMET_SMALL: pushValue(vm, (int64_t)imm.smallVal); break;
        case COMET_INT: pushValue(vm, (int64_t)imm.intVal); break;
        case COMET_BIG: pushValue(vm, (int64_t)imm.bigVal); break;
        case COMET_FLOAT: {
            int64_t casted;
            memcpy(&casted, &imm.floatVal, sizeof(float));
            pushValue(vm, casted);
            break;
        }
        case COMET_DOUBLE: {
            int64_t casted;
            memcpy(&casted, &imm.doubleVal, sizeof(double));
            pushValue(vm, casted);
            break;
        }
        case COMET_BOOL: pushValue(vm, (int64_t)imm.boolVal); break;
        case COMET_STRUCT: pushValue(vm, (int64_t)imm.objectVal); break;
        case COMET_FUNCTION: pushValue(vm, (int64_t)imm.smallVal); break;
        case COMET_VOID: break;
        default:
            assert(imm.typeKind != COMET_VOID);
            break;
    }
}

FORCE_INLINE void pushOperand(CometVM* vm, CometOperand operand) {
    switch (operand.type) {
        case CO_IMMEDIATE: {
            pushImm(vm, operand.imm);
            break;
        }

        default: break;
    }
}

CometSerializedFunc* findFunctionByName(CometVM* vm, char* name) {
    for (size_t i = 0; i < vm->numFunctions; i++) {
        if (strcmp(vm->functions[i].name, name) == 0) {
            return &vm->functions[i];
        }
    }

    return NULL;
}

void callFunction(CometVM* vm, CometSerializedFunc* function, uint8_t callArgs) {
    uint8_t numArgs = function->isVarArgs ? callArgs : function->numArgs;

    if (function->isExternal) {
        int64_t args[numArgs];

        for (int8_t argIdx = numArgs; argIdx > 0; argIdx--) {
            args[argIdx - 1] = popValue(vm);
        }
        ResultType(int64_t, objectPtr) returnValue = vm->externalFuncs[function->externFuncIndex](args, vm);
        if (returnValue.error) {
            CometObject* obj = returnValue.as.error;
            vmThrow(vm, (char*)obj->fields[0], (char*)obj->fields[1], obj);
        }

        pushValue(vm, returnValue.as.success);

        return;
    }

    Frame* newFrame = &vm->callStack[vm->callIdx++];

    newFrame->ip = function->startIdx;
    newFrame->funcName = function->name;

    for (size_t i = numArgs; i > 0; i--) {
        newFrame->args[i - 1] = popValue(vm);
    }

    vm->currentFrame = newFrame;
}

void returnFromFunc(CometVM* vm) {
    vm->callIdx--;

    if (vm->callIdx == 0) {
        vm->running = false;
        return;
    }

    vm->currentFrame = &vm->callStack[vm->callIdx-1];
}

void buildList(CometVM* vm) {
    // get size
    int64_t size = popValue(vm);

    if (size < 0) {
        char* trace = stackTrace(vm);

        fprintf(stderr, "Attempted to create a list of negative size!\n%s", trace);
        assert(size < 0);
    }

    // create list object
    CometSerializedArray* array = malloc(sizeof(CometSerializedArray));

    // malloc list
    int64_t* arrayData = calloc(size, sizeof(int64_t));

    // put values into array
    for (int64_t i = size; i > 0; i--) {
        arrayData[i - 1] = popValue(vm);
    }

    array->data = arrayData;
    array->capacity = (uint64_t)size;
    array->elemType = (CometType){ .typeKind = COMET_SMALL };

    // push array onto stack
    pushValue(vm, (int64_t)array);
}

FORCE_INLINE CometSerializedInst fetchNextInst(CometVM* vm) {
    return vm->instructions[vm->currentFrame->ip++];
}

ResultType(voidPtr, charptr) invalidInstruction(CometSerializedInst inst) {
    char* buffer = malloc(128);
    sprintf(buffer, "Reached invalid instruction! (%d)", inst.opcode);
    return Error(voidPtr, charptr, buffer);
}

void vmThrow(CometVM* vm, char* errName, char* msg, CometObject* errPtr) {
    if (vm->currentExcept == 0) {
        char* trace = stackTrace(vm);

        if (!vm->debugInfo) {
            ErrorMessage errMsg = createError(
                "<empty>",
                "<bytecode>",
                errName,
                msg,
                "No debug info was compiled with program. Add -d to your compiler flags to include debug symbols.",
                0,
                0,
                0
            );

            printErrorMessage(errMsg);
        } else {
            ErrorMessage errMsg = createError(
                vm->debugInfo->fileName,
                vm->debugInfo->sourceCode,
                errName,
                msg,
                NULL,
                vm->debugInfo->instructions[vm->currentFrame->ip],
                0,
                0
            );
            printErrorMessage(errMsg);
        }

        
        exit(1);
    }

    ExceptFrame frame = vm->exceptStack[--vm->currentExcept];
    vm->sp = frame.restoredSP;
    vm->currentFrame->ip = frame.handlerIP;

    pushValue(vm, (int64_t)errPtr);
}

ResultType(voidPtr, charptr) vmMainLoop(CometVM* vm) {
    CometSerializedInst inst;

    // computed goto table
    static const void* dispatchTable[] = {
        &&PUSH_CONST,
        &&STORE,
        &&LOAD,
        &&ADDI,
        &&ADDF,
        &&SUBI,
        &&SUBF,
        &&MULI,
        &&MULF,
        &&DIVI,
        &&DIVF,
        &&EQI,
        &&EQF,
        &&NEQI,
        &&NEQF,
        &&GTI,
        &&GTF,
        &&LTI,
        &&LTF,
        &&GTEI,
        &&GTEF,
        &&LTEI,
        &&LTEF,
        &&LOAD_ARG,
        &&RET,
        &&CALL,
        &&JMP,
        &&JMP_IF_FALSE,
        &&JMP_IF_TRUE,
        &&NOT,
        &&I2F,
        &&F2I,
        &&DUP,
        &&NEW,
        &&GET_FIELD,
        &&SET_FIELD,
        &&CALL_METHOD,
        &&BREAKPOINT,
        &&BUILD_LIST,
        &&LIST_AT,
        &&LIST_SET,
        &&TRY,
        &&END_TRY,
        &&THROW,
        &&LIST_LENGTH
    };

    #define DISPATCH()  if (!vm->running) { \
                            return Success(voidPtr, charptr, NULL); \
                        } \
                         \
                        if (vm->breakpoints[vm->currentFrame->ip] == 1) { \
                            startDebugger(vm, false); \
                        } \
                        if (vm->instructionsLeftToExec == 0) { \
                            startDebugger(vm, true); \
                        } \
                        vm->instructionsLeftToExec--; \
                        inst = fetchNextInst(vm); \
                         \
                        if (inst.opcode < 0 || inst.opcode > INST_MAX) { \
                            return invalidInstruction(inst); \
                        } \
                         \
                        goto *dispatchTable[inst.opcode];

    DISPATCH();

    PUSH_CONST: {
        CometOperand value = vm->constants[inst.a];
        
        pushOperand(vm, value);
        DISPATCH();
    }
    ADDI: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        pushValue(vm, a + b);
        DISPATCH();
    }
    ADDF: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        double aDouble;
        memcpy(&aDouble, &a, sizeof(double));
        double bDouble;
        memcpy(&bDouble, &b, sizeof(double));

        double result = aDouble + bDouble;
        int64_t outputtedResult;
        memcpy(&outputtedResult, &result, sizeof(int64_t));

        pushValue(vm, outputtedResult);
        DISPATCH();
    }
    SUBI: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        pushValue(vm, a - b);
        DISPATCH();
    } 
    SUBF: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        double aDouble;
        memcpy(&aDouble, &a, sizeof(double));
        double bDouble;
        memcpy(&bDouble, &b, sizeof(double));

        double result = aDouble - bDouble;
        int64_t outputtedResult;
        memcpy(&outputtedResult, &result, sizeof(int64_t));

        pushValue(vm, outputtedResult);
        DISPATCH();
    } 
    MULI: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        pushValue(vm, a * b);
        DISPATCH();
    }
    MULF: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        double aDouble;
        memcpy(&aDouble, &a, sizeof(double));
        double bDouble;
        memcpy(&bDouble, &b, sizeof(double));

        double result = aDouble * bDouble;
        int64_t outputtedResult;
        memcpy(&outputtedResult, &result, sizeof(int64_t));

        pushValue(vm, outputtedResult);
        DISPATCH();
    }
    DIVI: {
        int64_t b = popValue(vm);

        if (b == 0) {
            vmThrow(vm, "DivisionByZero", "Division by zero", NULL);
            DISPATCH();
        }

        int64_t a = popValue(vm);

        double result = (double)a / (double)b;
        int64_t casted;
        memcpy(&casted, &result, sizeof(int64_t));

        pushValue(vm, a / b);
        DISPATCH();
    }
    DIVF: {
        int64_t b = popValue(vm);

        double bDouble;
        memcpy(&bDouble, &b, sizeof(double));

        if (bDouble == 0) {
            vmThrow(vm, "DivisionByZero", "Division by zero", NULL);
            DISPATCH();
        }

        int64_t a = popValue(vm);

        double aDouble;
        memcpy(&aDouble, &a, sizeof(double));

        double result = aDouble / bDouble;
        int64_t outputtedResult;
        memcpy(&outputtedResult, &result, sizeof(int64_t));

        pushValue(vm, outputtedResult);
        DISPATCH();
    }
    EQI: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        pushValue(vm, a == b);
        DISPATCH();
    }
    EQF: {
        int64_t b = popValue(vm);
        double bDouble;
        memcpy(&bDouble, &b, sizeof(double));

        int64_t a = popValue(vm);
        double aDouble;
        memcpy(&aDouble, &a, sizeof(double));

        pushValue(vm, aDouble == bDouble);
        DISPATCH();
    }
    NEQI: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        pushValue(vm, a != b);
        DISPATCH();
    }
    NEQF: {
        int64_t b = popValue(vm);
        double bDouble;
        memcpy(&bDouble, &b, sizeof(double));

        int64_t a = popValue(vm);
        double aDouble;
        memcpy(&aDouble, &a, sizeof(double));

        pushValue(vm, aDouble != bDouble);
        DISPATCH();
    }
    LTI: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        pushValue(vm, a < b);
        DISPATCH();
    }
    LTF: {
        int64_t b = popValue(vm);
        double bDouble;
        memcpy(&bDouble, &b, sizeof(double));

        int64_t a = popValue(vm);
        double aDouble;
        memcpy(&aDouble, &a, sizeof(double));

        pushValue(vm, aDouble < bDouble);
        DISPATCH();
    }
    GTI: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        pushValue(vm, a > b);
        DISPATCH();
    }
    GTF: {
        int64_t b = popValue(vm);
        double bDouble;
        memcpy(&bDouble, &b, sizeof(double));

        int64_t a = popValue(vm);
        double aDouble;
        memcpy(&aDouble, &a, sizeof(double));

        pushValue(vm, aDouble > bDouble);
        DISPATCH();
    }
    LTEI: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        pushValue(vm, a <= b);
        DISPATCH();
    }
    LTEF: {
        int64_t b = popValue(vm);
        double bDouble;
        memcpy(&bDouble, &b, sizeof(double));

        int64_t a = popValue(vm);
        double aDouble;
        memcpy(&aDouble, &a, sizeof(double));

        pushValue(vm, aDouble <= bDouble);
        DISPATCH();
    }
    GTEI: {
        int64_t b = popValue(vm);
        int64_t a = popValue(vm);

        pushValue(vm, a >= b);
        DISPATCH();
    }
    GTEF: {
        int64_t b = popValue(vm);
        double bDouble;
        memcpy(&bDouble, &b, sizeof(double));

        int64_t a = popValue(vm);
        double aDouble;
        memcpy(&aDouble, &a, sizeof(double));

        pushValue(vm, aDouble >= bDouble);
        DISPATCH();
    }
    JMP: {
        vm->currentFrame->ip = inst.a;
        DISPATCH();
    }
    JMP_IF_FALSE: {
        if (!popValue(vm)) {
            vm->currentFrame->ip = inst.a;
        }
        DISPATCH();
    }
    JMP_IF_TRUE: {
        if (popValue(vm)) {
            vm->currentFrame->ip = inst.a;
        }
        DISPATCH();
    }
    STORE: {
        vm->variables[inst.a] = popValue(vm);
        DISPATCH();
    } 
    LOAD: {
        pushValue(vm, vm->variables[inst.a]);
        DISPATCH();
    }
    LOAD_ARG: {
        pushValue(vm, vm->currentFrame->args[inst.a]); // 🍌 - i just kinda felt like adding this here
        DISPATCH();
    }
    RET: {
        returnFromFunc(vm);
        DISPATCH();
    }
    CALL: {
        callFunction(vm, &vm->functions[inst.a], inst.b);
        DISPATCH();
    }
    NOT: {
        pushValue(vm, !popValue(vm));
        DISPATCH();
    }
    I2F: {
        double value = popValue(vm);

        int64_t casted;
        memcpy(&casted, &value, sizeof(int64_t));

        pushValue(vm, casted);
        DISPATCH();
    }
    F2I: {
        int64_t value = popValue(vm);

        double floatVal;
        memcpy(&floatVal, &value, sizeof(double));

        int64_t intVal = floatVal;

        pushValue(vm, intVal);
        DISPATCH();
    }
    DUP: {
        pushValue(vm, getTop(vm));
        DISPATCH();
    }
    NEW: {
        CometObject* newObj = malloc(sizeof(CometObject));

        newObj->vtable = vm->structs[inst.a].vtable;
        newObj->fields = calloc(vm->structs[inst.a].numFields, sizeof(int64_t));
        
        pushValue(vm, (int64_t)newObj);
        DISPATCH();
    }
    GET_FIELD: {
        CometObject* obj = (CometObject*)popValue(vm);

        pushValue(vm, obj->fields[inst.a]);
        DISPATCH();
    }
    SET_FIELD: {
        CometObject* obj = (CometObject*)popValue(vm);
        obj->fields[inst.a] = popValue(vm);
        DISPATCH();
    }
    CALL_METHOD: {
        CometObject* obj = (CometObject*)getTop(vm);

        uint32_t methodIdx = inst.a;
        CometSerializedFunc func = vm->functions[obj->vtable[methodIdx]];

        callFunction(vm, &func, inst.b);

        
        DISPATCH();
    }
    BUILD_LIST: {
        buildList(vm);
        DISPATCH();
    }
    LIST_AT: {
        int64_t index = popValue(vm);
        CometSerializedArray* array = (CometSerializedArray*)popValue(vm);

        uint64_t capacity = array->capacity;

        if (index < 0) index = capacity + index;
        if (index < 0 || index >= capacity) {
            char* buffer = malloc(128);
            snprintf(buffer, 128, "Index %ld out of bounds for array of size %lu.\n", index, capacity);
            return Error(voidPtr, charptr, buffer);
        }

        pushValue(
            vm, 
            array->data[index]
        );
        DISPATCH();
    }
    LIST_SET: {
        int64_t index = popValue(vm);
        CometSerializedArray* array = (CometSerializedArray*)popValue(vm);
        int64_t newValue = popValue(vm);

        uint64_t capacity = array->capacity;

        if (index < 0) index = capacity + index;
        if (index < 0 || index >= capacity) {
            char* buffer = malloc(128);
            snprintf(buffer, 128, "Index %ld out of bounds for array of size %lu.\n", index, capacity);
            return Error(voidPtr, charptr, buffer);
        }

        array->data[index] = newValue;
        DISPATCH();
    }
    TRY: {
        vm->exceptStack[vm->currentExcept++] = (ExceptFrame){
            .handlerIP = popValue(vm),
            .restoredSP = vm->sp
        };
        DISPATCH();
    }
    END_TRY: {
        vm->currentExcept--;
        DISPATCH();
    }
    THROW: {
        CometObject* exception = (CometObject*)popValue(vm);

        CometSerializedArray* exceptNameArr = (CometSerializedArray*)exception->fields[0];
        char* exceptName = malloc(exceptNameArr->capacity);
        for (size_t i = 0; i < exceptNameArr->capacity; i++) {
            exceptName[i] = exceptNameArr->data[i];
        }

        CometSerializedArray* exceptMsgArr = (CometSerializedArray*)exception->fields[1];
        char* exceptMsg = malloc(exceptMsgArr->capacity);
        for (size_t i = 0; i < exceptMsgArr->capacity; i++) {
            exceptMsg[i] = exceptMsgArr->data[i];
        }

        vmThrow(vm, exceptName, exceptMsg, exception);
        DISPATCH();
    }
    LIST_LENGTH: {
        CometSerializedArray* array = (CometSerializedArray*)popValue(vm);
        pushValue(vm, (int64_t)array->capacity);

        DISPATCH();
    }
    BREAKPOINT: {
        createBreakpoint(vm);
        startDebugger(vm, false);
        DISPATCH();
    }


    return Success(voidPtr, charptr, NULL);
}

ResultType(int, charptr) startVM(CometVM* vm) {
    vm->running = true;

    // find main function
    CometSerializedFunc* mainFunc = findFunctionByName(vm, "main");
    if (!mainFunc) {
        return Error(int, charptr, "Could not find main function!");
    }
    callFunction(vm, mainFunc, 0);

    ResultType(voidPtr, charptr) loopResult = vmMainLoop(vm);
    if (loopResult.error) {
        Estr errMsg = CREATE_ESTR(loopResult.as.error);
        APPEND_ESTR(errMsg, stackTrace(vm));

        return Error(int, charptr, errMsg.str);
    }

    if (vm->sp <= 0) {
        return Success(int, charptr, 0);
    }

    return Success(int, charptr, getTop(vm));
}

ResultType(vmPtr, charptr) newCometVM(char* filePath) {
    CometFile* loadedFile = (CometFile*)getFileContents(filePath);

    // header magic isnt correct
    char magic[5] = {'C','O','M','E','T'};
    if (memcmp(loadedFile->magic, magic, 5) != 0) {
        return Error(vmPtr, charptr, "given file is not a comet file!");
    }

    CometVM* newVM = malloc(sizeof(CometVM));
    if (newVM == NULL) {
        free(loadedFile);
        return Error(vmPtr, charptr, "failed to allocate memory for CometVM!");
    }

    // allocate arrays
    newVM->numConstants = loadedFile->numConsts;
    newVM->numFunctions = loadedFile->numFunctions;
    newVM->numStructs = loadedFile->numStructs;
    newVM->numInstructions = loadedFile->numInstructions;

    newVM->instructions = calloc(loadedFile->numInstructions, sizeof(CometSerializedInst));
    newVM->constants = calloc(loadedFile->numConsts, sizeof(CometOperand));
    newVM->structs = calloc(loadedFile->numStructs, sizeof(CometSerializedStruct));
    newVM->functions = calloc(loadedFile->numFunctions, sizeof(CometSerializedFunc));

    char* cursor = ((char*)loadedFile) + sizeof(CometFile);

    size_t constantsTableSize = sizeof(CometOperand) * loadedFile->numConsts;
    size_t functionsTableSize = sizeof(CometSerializedFunc) * loadedFile->numFunctions;

    // constants
    memcpy(newVM->constants,
        cursor,
        constantsTableSize);
    cursor += constantsTableSize;

    // functions
    memcpy(newVM->functions,
        cursor,
        functionsTableSize);
    cursor += functionsTableSize;

    // structs
    for (uint32_t i = 0; i < loadedFile->numStructs; i++) {
        uint32_t numFields;
        uint32_t numMethods;
        char name[48];

        memcpy(name, cursor, 48);
        cursor += 48;

        memcpy(&numFields, cursor, sizeof(uint32_t));
        cursor += sizeof(uint32_t);

        memcpy(&numMethods, cursor, sizeof(uint32_t));
        cursor += sizeof(uint32_t);

        newVM->structs[i].numFields = numFields;
        newVM->structs[i].numMethods = numMethods;
        memcpy(newVM->structs[i].name, name, 48);

        newVM->structs[i].vtable =
            malloc(sizeof(uint32_t) * numMethods);

        memcpy(newVM->structs[i].vtable,
            cursor,
            sizeof(uint32_t) * numMethods);

        cursor += sizeof(uint32_t) * numMethods;
    }

    newVM->loadedLibs = calloc(loadedFile->numLibs, sizeof(void*));
    newVM->externalFuncs = calloc(loadedFile->numFunctions, sizeof(void*));

    newVM->numExternalFuncs = 0;
    for (size_t i = 0; i < loadedFile->numLibs; i++) {
        char libName[128];
        snprintf(libName, 128, "%s.cometlib", cursor);

        char* cometLibsPath = getLibsDir();
        if (!cometLibsPath) {
            cometLibsPath = "";
        }

        char libPath[1024];
        snprintf(libPath, 1024, "%s/%s", cometLibsPath, libName);

        void* handle = dlopen(libPath, RTLD_NOW);
        if (!handle) {    
            free(newVM->instructions);
            free(newVM->constants);
            free(newVM->structs);
            free(newVM->functions);
            free(newVM);

            const char* err = dlerror();

            Estr errMsg = CREATE_ESTR("failed to load external lib \"");
            APPEND_ESTR(errMsg, libPath);
            APPEND_ESTR(errMsg, "\": ");
            APPEND_ESTR(errMsg, err);
            return Error(vmPtr, charptr, errMsg.str);
        }

        newVM->loadedLibs[i] = handle;

        // load each function
        for (size_t symbolIdx = 0; symbolIdx < newVM->numFunctions; symbolIdx++) {
            CometSerializedFunc* externalFunc = &newVM->functions[symbolIdx];

            if (!(externalFunc->libIdx == i && externalFunc->isExternal))
                continue;
            

            Estr funcSymbolName = CREATE_ESTR("impl_");
            APPEND_ESTR(funcSymbolName, externalFunc->name);

            void* loadedFunc = dlsym(handle, funcSymbolName.str);
            if (!loadedFunc) {
                const char* err = dlerror();

                DESTROY_ESTR(funcSymbolName);

                Estr errMsg = CREATE_ESTR("failed to load func \"");
                APPEND_ESTR(errMsg, externalFunc->name);
                APPEND_ESTR(errMsg, "\" from external library \"");
                APPEND_ESTR(errMsg, libPath);
                APPEND_ESTR(errMsg, "\": ");
                APPEND_ESTR(errMsg, err);
                return Error(vmPtr, charptr, errMsg.str);
            }

            DESTROY_ESTR(funcSymbolName);

            externalFunc->externFuncIndex = newVM->numExternalFuncs;
            newVM->externalFuncs[newVM->numExternalFuncs] = loadedFunc;
            newVM->numExternalFuncs++;
            

            
        } 

        cursor += 64;
    }

    // instructions
    memcpy(newVM->instructions,
       cursor,
       sizeof(CometSerializedInst) * loadedFile->numInstructions);    
    cursor += sizeof(CometSerializedInst) * loadedFile->numInstructions;

    newVM->currentFrame = NULL,
    newVM->callIdx = 0;

    // debug symbols
    bool hasDebugInfo = *cursor;
    cursor++;

    if (hasDebugInfo) {
        DebugInfo* dbgInfo = malloc(sizeof(DebugInfo));
        memcpy(dbgInfo->fileName, cursor, 32);
        cursor += 32;

        size_t sourceLen;
        memcpy(&sourceLen, cursor, sizeof(size_t));
        cursor += sizeof(size_t);

        dbgInfo->sourceCode = cursor;
        memcpy(dbgInfo->sourceCode, cursor, sourceLen);
        cursor += sourceLen;

        dbgInfo->instructions = malloc(sizeof(uint64_t) * newVM->numInstructions);

        memcpy(dbgInfo->instructions, cursor, sizeof(uint64_t) * newVM->numInstructions);
        newVM->debugInfo = dbgInfo;
    } else {
        newVM->debugInfo = NULL;
    }

    newVM->instructionsLeftToExec = UINT64_MAX;
    newVM->breakpoints = calloc(newVM->numInstructions, sizeof(uint8_t));

    newVM->currentExcept = 0;

    return Success(vmPtr, charptr, newVM);
}