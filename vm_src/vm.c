#include "serialized.h"
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vm.h"
#include "args.h"
#include "../include/operand.h"
#include "../lib/estr.h"
#include "debugger.h"

// For Clang and GCC on macOS
#define FORCE_INLINE __attribute__((always_inline)) static inline

CometFile* getFileContents(const char* filename) {
    // https://stackoverflow.com/questions/3747086/reading-the-whole-text-file-into-a-char-array-in-c
    FILE* fp;
    size_t lSize;
    CometFile* file;

    fp = fopen(filename, "rb");
    if (!fp) {
        perror(filename);
        exit(1);
    }

    fseek(fp, 0L, SEEK_END);
    lSize = ftell(fp);
    rewind(fp);

    if (lSize < sizeof(CometFile)) {
        fclose(fp);
        fprintf(stderr, "file %s is too small to be a comet file\n", filename);
        exit(1);
    }

    file = calloc(1, lSize);
    if (!file) {
        fclose(fp);
        fprintf(stderr, "memory allocation fail when reading file %s\n", filename);
        exit(1);
    }

    if (1!=fread(file, lSize, 1, fp)) {
        fclose(fp);
        free(file);
        fputs("couldn't read entire file", stderr);
        exit(1);
    }

    // we done
    fclose(fp);

    return file;
}

static inline int max(int a, int b) {
    return (a > b) ? a : b;
}

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
        assert(false);
    }

    return vm->stack[vm->sp-1];
}

FORCE_INLINE int64_t popValue(CometVM* vm) {
    int64_t value = getTop(vm);
    vm->sp--;
    return value;
}

CometOperand getConst(CometVM* vm, uint32_t idx) {
    return vm->constants[idx];
}

void pushImm(CometVM* vm, CometImmediate imm) {
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
    }
}

void pushOperand(CometVM* vm, CometOperand operand) {
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

void callFunction(CometVM* vm, CometSerializedFunc* function) {
    Frame* newFrame = &vm->callStack[vm->callIdx++];

    newFrame->ip = function->startIdx;
    //newFrame.stackStart = vm->sp;
    newFrame->funcName = function->name;

    for (size_t i = function->numArgs; i > 0; i--) {
        newFrame->args[i - 1] = popValue(vm);
    }

    vm->currentFrame = newFrame;
}

void returnFromFunc(CometVM* vm) {
    Frame funcFrame = vm->callStack[vm->callIdx - 1];
    vm->callIdx--;

    if (vm->callIdx == 0) {
        vm->running = false;
        return;
    }

    vm->currentFrame = &vm->callStack[vm->callIdx-1];
}

FORCE_INLINE CometSerializedInst fetchNextInst(CometVM* vm) {
    return vm->instructions[vm->currentFrame->ip++];
}

ResultType(voidPtr, charptr) invalidInstruction(CometSerializedInst inst) {
    char* buffer = malloc(128);
    sprintf(buffer, "Reached invalid instruction! (%d)", inst.opcode);
    return Error(voidPtr, charptr, buffer);
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
        &&DUP,
        &&NEW,
        &&GET_FIELD,
        &&SET_FIELD,
        &&CALL_METHOD,
        &&BREAKPOINT
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
        CometOperand value = getConst(vm, inst.a);
        
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
            return Error(voidPtr, charptr, "Division by zero");
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
            return Error(voidPtr, charptr, "Division by zero");
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
        callFunction(vm, &vm->functions[inst.a]);
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
        uint32_t symbolIdx = obj->vtable[methodIdx];
        CometSerializedFunc* func = &vm->functions[symbolIdx];

        callFunction(vm, func);
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
    callFunction(vm, mainFunc);

    ResultType(voidPtr, charptr) loopResult = vmMainLoop(vm);
    if (loopResult.error) {
        Estr errMsg = CREATE_ESTR(loopResult.as.error);
        APPEND_ESTR(errMsg, stackTrace(vm));

        return Error(int, charptr, errMsg.str);
    }

    return Success(int, charptr, vm->stack[vm->sp-1]);//(*vm->currentStack)[(*vm->currentSp)-1]);
}



ResultType(vmPtr, charptr) newCometVM(char* filePath) {
    CometFile* loadedFile = getFileContents(filePath);

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

        memcpy(&numFields, cursor, sizeof(uint32_t));
        cursor += sizeof(uint32_t);

        memcpy(&numMethods, cursor, sizeof(uint32_t));
        cursor += sizeof(uint32_t);

        newVM->structs[i].numFields = numFields;
        newVM->structs[i].numMethods = numMethods;

        newVM->structs[i].vtable =
            malloc(sizeof(uint32_t) * numMethods);

        memcpy(newVM->structs[i].vtable,
            cursor,
            sizeof(uint32_t) * numMethods);

        cursor += sizeof(uint32_t) * numMethods;
    }

    // instructions
    memcpy(newVM->instructions,
       cursor,
       sizeof(CometSerializedInst) * loadedFile->numInstructions);

    newVM->numConstants = loadedFile->numConsts;
    newVM->numFunctions = loadedFile->numFunctions;
    newVM->numStructs = loadedFile->numStructs;
    newVM->numInstructions = loadedFile->numInstructions;

    newVM->currentFrame = NULL,
    newVM->callIdx = 0;

    newVM->instructionsLeftToExec = UINT64_MAX;
    newVM->breakpoints = calloc(newVM->numInstructions, sizeof(uint8_t));

    return Success(vmPtr, charptr, newVM);
}