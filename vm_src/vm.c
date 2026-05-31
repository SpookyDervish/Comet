#include "vm.h"
#include "args.h"
#include "../include/operand.h"
#include "serialized.h"
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../lib/estr.h"

CometFile* getFileContents(const char* filename) {
    // https://stackoverflow.com/questions/3747086/reading-the-whole-text-file-into-a-char-array-in-c
    FILE* fp;
    long lSize;
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

int max(int a, int b) {
    return (a > b) ? a : b;
}

void push(CometVM* vm, int64_t value) {
    (*vm->currentStack)[*vm->currentSp] = value;
    *vm->currentSp += 1;
}

char* stackAsString(int64_t* stack, uint32_t sp) {
    Estr stackString = CREATE_ESTR("[");

    for (size_t i = 0; i < sp; i++) {

        char* buffer = malloc(64);

        sprintf(buffer, "0x%" PRIx64 "%s", stack[i], i < sp-1 ? ", " : "");
        APPEND_ESTR(stackString, buffer);
    }

    APPEND_ESTR(stackString, "]");

    return stackString.str;
}

char* stackTrace(CometVM* vm) {
    Estr stackTrace = CREATE_ESTR("\nCall Stack (most recent call first):\n");

    for (size_t i = vm->callIdx; i > 0; i--) {
        Frame* call = vm->callStack[i];

        char* funcBuffer = malloc(128);
        sprintf(funcBuffer, "    0x%04lx    %s    (sp: 0x%x)  %s\n", call->ip, call->funcName, call->sp, stackAsString(call->stack, call->sp));

        APPEND_ESTR(stackTrace, funcBuffer);
    }

    return stackTrace.str;
}

int64_t getTop(CometVM* vm) {
    if ((*vm->currentSp) <= 0) {
        fprintf(stderr, "Attempted to pop top of stack while stack was empty, this is a compiler bug! Please report this at https://chookspace.com/Comet/Comet/issues with your code.\n");
        fprintf(stderr, "%s\n", stackTrace(vm));
        assert(false);
    }

    return (*vm->currentStack)[(*vm->currentSp)-1];
}

int64_t pop(CometVM* vm) {
    int64_t value = getTop(vm);
    *vm->currentSp -= 1;
    return value;
}

CometOperand getConst(CometVM* vm, uint32_t idx) {
    return vm->constants[idx];
}

void pushImm(CometVM* vm, CometImmediate imm) {
    switch (imm.typeKind) {
        case COMET_SMALL: push(vm, (int64_t)imm.smallVal); break;
        case COMET_INT: push(vm, (int64_t)imm.intVal); break;
        case COMET_BIG: push(vm, (int64_t)imm.bigVal); break;
        case COMET_FLOAT: {
            int64_t casted;
            memcpy(&casted, &imm.floatVal, sizeof(float));
            push(vm, casted);
            break;
        }
        case COMET_DOUBLE: {
            int64_t casted;
            memcpy(&casted, &imm.doubleVal, sizeof(double));
            push(vm, casted);
            break;
        }
        case COMET_BOOL: push(vm, (int64_t)imm.boolVal); break;
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

    Frame* callFrame = malloc(sizeof(Frame));
    callFrame->stack = calloc(256, sizeof(int64_t));
    callFrame->args = calloc(256, sizeof(int64_t));
    callFrame->sp = 0;
    callFrame->funcName = function->name;
    callFrame->ip = function->startIdx;

    for (size_t i = function->numArgs; i > 0; i--) {
        callFrame->args[i - 1] = pop(vm);
    }

    vm->callStack[vm->callIdx] = callFrame;
    vm->callIdx++;

    vm->currentStack = &callFrame->stack;
    vm->currentFrame = callFrame;
    vm->currentSp = &callFrame->sp;

}

void returnFromFunc(CometVM* vm) {
    Frame* funcFrame = vm->callStack[vm->callIdx-1];
    vm->callIdx--;
    

    if (vm->callIdx == 0) {
        vm->running = false;
        return;
    }

    vm->currentFrame = vm->callStack[vm->callIdx-1];
    vm->currentStack = &vm->currentFrame->stack;
    *vm->currentSp = vm->currentFrame->sp+1;

    push(vm, funcFrame->stack[funcFrame->sp]);
    

    free(funcFrame);
}

ResultType(voidPtr, charptr) vmClock(CometVM* vm) {
    CometSerializedInst inst = vm->instructions[vm->currentFrame->ip];
    vm->currentFrame->ip++;

    switch (inst.opcode) {
        case INST_PUSH_CONST: {
            CometOperand value = getConst(vm, inst.a);
            
            pushOperand(vm, value);
            break;
        }

        case INST_ADDI: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a + b);
            break;
        }
        case INST_ADDF: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            double aDouble;
            memcpy(&aDouble, &a, sizeof(double));
            double bDouble;
            memcpy(&bDouble, &b, sizeof(double));

            double result = aDouble + bDouble;
            int64_t outputtedResult;
            memcpy(&outputtedResult, &result, sizeof(int64_t));

            push(vm, outputtedResult);
            break;
        }
        case INST_SUBI: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a - b);
            break;
        } 
        case INST_SUBF: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            double aDouble;
            memcpy(&aDouble, &a, sizeof(double));
            double bDouble;
            memcpy(&bDouble, &b, sizeof(double));

            double result = aDouble - bDouble;
            int64_t outputtedResult;
            memcpy(&outputtedResult, &result, sizeof(int64_t));

            push(vm, outputtedResult);
            break;
        } 
        case INST_MULI: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a * b);
            break;
        }
        case INST_MULF: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            double aDouble;
            memcpy(&aDouble, &a, sizeof(double));
            double bDouble;
            memcpy(&bDouble, &b, sizeof(double));

            double result = aDouble * bDouble;
            int64_t outputtedResult;
            memcpy(&outputtedResult, &result, sizeof(int64_t));

            push(vm, outputtedResult);
            break;
        }
        case INST_DIVI: {
            int64_t b = pop(vm);

            if (b == 0) {
                return Error(voidPtr, charptr, "Division by zero");
            }

            int64_t a = pop(vm);

            double result = (double)a / (double)b;
            int64_t casted;
            memcpy(&casted, &result, sizeof(int64_t));

            push(vm, a * b);
            break;
        }
        case INST_DIVF: {
            int64_t b = pop(vm);

            double bDouble;
            memcpy(&bDouble, &b, sizeof(double));

            if (bDouble == 0) {
                return Error(voidPtr, charptr, "Division by zero");
            }

            int64_t a = pop(vm);

            double aDouble;
            memcpy(&aDouble, &a, sizeof(double));

            double result = aDouble / bDouble;
            int64_t outputtedResult;
            memcpy(&outputtedResult, &result, sizeof(int64_t));

            push(vm, outputtedResult);
            break;
        }

        case INST_EQI: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a == b);
            break;
        }
        case INST_EQF: {
            int64_t b = pop(vm);
            double bDouble;
            memcpy(&bDouble, &b, sizeof(double));

            int64_t a = pop(vm);
            double aDouble;
            memcpy(&aDouble, &a, sizeof(double));

            push(vm, aDouble == bDouble);
            break;
        }
        case INST_NEQI: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a != b);
            break;
        }
        case INST_NEQF: {
            int64_t b = pop(vm);
            double bDouble;
            memcpy(&bDouble, &b, sizeof(double));

            int64_t a = pop(vm);
            double aDouble;
            memcpy(&aDouble, &a, sizeof(double));

            push(vm, aDouble != bDouble);
            break;
        }
        case INST_LTI: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a < b);
            break;
        }
        case INST_LTF: {
            int64_t b = pop(vm);
            double bDouble;
            memcpy(&bDouble, &b, sizeof(double));

            int64_t a = pop(vm);
            double aDouble;
            memcpy(&aDouble, &a, sizeof(double));

            push(vm, aDouble < bDouble);
            break;
        }
        case INST_GTI: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a > b);
            break;
        }
        case INST_GTF: {
            int64_t b = pop(vm);
            double bDouble;
            memcpy(&bDouble, &b, sizeof(double));

            int64_t a = pop(vm);
            double aDouble;
            memcpy(&aDouble, &a, sizeof(double));

            push(vm, aDouble > bDouble);
            break;
        }
        case INST_LTEI: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a <= b);
            break;
        }
        case INST_LTEF: {
            int64_t b = pop(vm);
            double bDouble;
            memcpy(&bDouble, &b, sizeof(double));

            int64_t a = pop(vm);
            double aDouble;
            memcpy(&aDouble, &a, sizeof(double));

            push(vm, aDouble <= bDouble);
            break;
        }
        case INST_GTEI: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a >= b);
            break;
        }
        case INST_GTEF: {
            int64_t b = pop(vm);
            double bDouble;
            memcpy(&bDouble, &b, sizeof(double));

            int64_t a = pop(vm);
            double aDouble;
            memcpy(&aDouble, &a, sizeof(double));

            push(vm, aDouble >= bDouble);
            break;
        }

        case INST_JMP: {
            vm->currentFrame->ip = inst.a;
            break;
        }

        case INST_JMP_IF_FALSE: {
            int64_t a = pop(vm);


            if (!a) {
                vm->currentFrame->ip = inst.a;
            }
            break;
        }

        case INST_STORE: {
            int64_t value = pop(vm);
            
            vm->variables[inst.a] = value;
            break;
        } 

        case INST_LOAD: {
            int64_t value = vm->variables[inst.a];
            push(vm, value);
            break;
        }

        case INST_CALL: {
            
            CometSerializedFunc function = vm->functions[inst.a];
            callFunction(vm, &function);
            break;
        }

        case INST_LOAD_ARG: {
            Frame* currentFrame = vm->callStack[vm->callIdx-1];
            push(vm, currentFrame->args[inst.a]);
            break;
        }

        case INST_RET: {
            returnFromFunc(vm);
            break;
        }

        case INST_NOT: {
            int64_t value = pop(vm);
            push(vm, !value);
            break;
        }

        case INST_I2F: {
            double value = pop(vm);

            int64_t casted;
            memcpy(&casted, &value, sizeof(int64_t));

            push(vm, casted);
            break;
        }

        case INST_DUP: {
            int64_t value = getTop(vm);

            push(vm, value);
            push(vm, value);
            break;
        }

        case INST_NEW: {
            CometObject* newObj = malloc(sizeof(CometObject));

            newObj->vtable = vm->structs[inst.a].vtable;
            newObj->fields = calloc(vm->structs[inst.a].numFields, sizeof(int64_t));
            
            push(vm, (int64_t)newObj);

            break;
        }

        case INST_GET_FIELD: {
            CometObject* obj = (CometObject*)pop(vm);

            push(vm, obj->fields[inst.a]);
            break;
        }
        case INST_SET_FIELD: {
            CometObject* obj = (CometObject*)pop(vm);
            int64_t newValue = pop(vm);

            obj->fields[inst.a] = newValue;
            break;
        }

        default: {
            char* buffer = malloc(128);
            sprintf(buffer, "Reached invalid instruction! (%d)", inst.opcode);
            return Error(voidPtr, charptr, buffer);
        };
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

    while (vm->running) {
        ResultType(voidPtr, charptr) instResult = vmClock(vm);
        if (instResult.error) {
            Estr errMsg = CREATE_ESTR(instResult.as.error);
            APPEND_ESTR(errMsg, stackTrace(vm));

            return Error(int, charptr, errMsg.str);
        }
    }

    return Success(int, charptr, (*vm->currentStack)[(*vm->currentSp)-1]);
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

    

    newVM->stackCapacity = max(max(64, loadedFile->numConsts), loadedFile->numConsts*2);
    newVM->instructions = calloc(loadedFile->numInstructions, sizeof(CometSerializedInst));
    newVM->constants = calloc(loadedFile->numConsts, sizeof(CometOperand));
    newVM->structs = calloc(loadedFile->numStructs, sizeof(CometSerializedStruct));
    newVM->functions = calloc(loadedFile->numFunctions, sizeof(CometSerializedFunc));

    char* cursor = ((char*)loadedFile) + sizeof(CometFile);

    size_t constantsTableSize = sizeof(CometOperand) * loadedFile->numConsts;
    size_t functionsTableSize = sizeof(CometSerializedFunc) * loadedFile->numFunctions;
    size_t structsTableSize = sizeof(CometSerializedStruct) * loadedFile->numStructs;

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
    newVM->variables = calloc(256, sizeof(int64_t));
    newVM->callStack = calloc(128, sizeof(Frame));
    newVM->currentSp = malloc(sizeof(uint32_t));

    *newVM->currentSp = 0;
    newVM->callIdx = 0;

    return Success(vmPtr, charptr, newVM);
}