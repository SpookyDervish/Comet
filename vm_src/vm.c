#include "vm.h"
#include "args.h"
#include "operand.h"
#include "serialized.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

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

ResultType(vmPtr, charptr) newCometVM(char* filePath) {
    CometFile* loadedFile = getFileContents(filePath);

    // header magic isnt correct
    char magic[5] = {'C','O','M','E','T'};
    if (memcmp(loadedFile->magic, magic, 5) != 0) {
        return Error(vmPtr, charptr, "given file is not a comet file!");
    }

    CometVM* newVM = malloc(sizeof(CometVM));
    if (newVM == NULL) {
        return Error(vmPtr, charptr, "failed to allocate memory for CometVM!");
    }

    newVM->stackCapacity = max(max(64, loadedFile->numConsts), loadedFile->numConsts*2);
    newVM->stack = calloc(newVM->stackCapacity, sizeof(int64_t));
    newVM->instructions = NULL;

    memcpy(newVM->constants, loadedFile + sizeof(CometFile), sizeof(CometOperand) * loadedFile->numConsts);

    printf("%d\n", newVM->constants[0].imm.intVal);

    return Success(vmPtr, charptr, newVM);
}