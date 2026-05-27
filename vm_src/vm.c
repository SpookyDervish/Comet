#include "vm.h"
#include "args.h"
#include "operand.h"
#include "serialized.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    ((*vm->currentStack)[*vm->currentSp]) = value;
    *vm->currentSp += 1;
}

int64_t pop(CometVM* vm) {
    int64_t value = (*vm->currentStack)[(*vm->currentSp)-1];
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
        case COMET_FLOAT: push(vm, (int64_t)imm.floatVal); break;
        case COMET_DOUBLE: push(vm, (int64_t)imm.doubleVal); break;
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

    for (size_t i = 0; i < function->numArgs; i++) {
        callFrame->args[i] = pop(vm);
    }

    vm->callStack[vm->callIdx] = callFrame;
    vm->callIdx++;

    vm->currentStack = &callFrame->stack;
    vm->currentFrame = callFrame;
    vm->currentFrame->ip = function->startIdx;

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
    *vm->currentSp = vm->currentFrame->sp;
\
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

        case INST_ADD: {
            int64_t a = pop(vm);
            int64_t b = pop(vm);

            push(vm, a + b);
            break;
        }

        case INST_SUB: {
            int64_t a = pop(vm);
            int64_t b = pop(vm);

            push(vm, a - b);
            break;
        } 

        case INST_MUL: {
            int64_t a = pop(vm);
            int64_t b = pop(vm);

            push(vm, a * b);
            break;
        }

        case INST_EQ: {
            int64_t a = pop(vm);
            int64_t b = pop(vm);

            push(vm, a == b);
            break;
        }
        case INST_LT: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            printf("a: %d, b: %d\n", a, b);

            push(vm, a < b);
            break;
        }
        case INST_GT: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a > b);
            break;
        }
        case INST_LTE: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a <= b);
            break;
        }
        case INST_GTE: {
            int64_t b = pop(vm);
            int64_t a = pop(vm);

            push(vm, a >= b);
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

        default: {
            char* buffer = malloc(128);
            sprintf(buffer, "Reached invalid instruction! (%d)", inst.opcode);
            return Error(voidPtr, charptr, buffer);
        }
    }
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
        if (instResult.error)
            return Error(int, charptr, instResult.as.error);
    }

   return Success(int, charptr, (*vm->currentStack)[0]);
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
    //newVM->currentStack = calloc(newvm->currentStackCapacity, sizeof(int64_t));
    newVM->instructions = calloc(loadedFile->numInstructions, sizeof(CometSerializedInst));
    newVM->constants = calloc(loadedFile->numConsts, sizeof(CometOperand));
    newVM->functions = calloc(loadedFile->numFunctions, sizeof(CometSerializedFunc));

    size_t constantsTableSize = sizeof(CometOperand) * loadedFile->numConsts;
    size_t functionsTableSize = sizeof(CometSerializedFunc) * loadedFile->numFunctions;
    memcpy(newVM->constants, ((char*)loadedFile) + sizeof(CometFile), constantsTableSize);
    memcpy(newVM->functions, ((char*)loadedFile) + sizeof(CometFile) + constantsTableSize, sizeof(CometSerializedFunc) * loadedFile->numFunctions);
    memcpy(newVM->instructions, ((char*)loadedFile) + sizeof(CometFile) + constantsTableSize + functionsTableSize, sizeof(CometSerializedInst) * loadedFile->numInstructions);

    newVM->numConstants = loadedFile->numConsts;
    newVM->numFunctions = loadedFile->numFunctions;
    newVM->variables = calloc(newVM->numConstants * 2, sizeof(int64_t));
    newVM->callStack = calloc(128, sizeof(Frame));
    newVM->currentSp = malloc(sizeof(uint32_t));

    *newVM->currentSp = 0;

    return Success(vmPtr, charptr, newVM);
}