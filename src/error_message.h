#ifndef ERROR_MESSAGE_H
#define ERROR_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../include/list.h"

typedef struct {
    char* fileName;
    char* sourceCode;
    char* message;  // "Unexpected token '#'"
    char* errType;  // "SyntaxError", "TypeError", etc...
    bool isWarning; // true or false
    char* help;     // can be NULL if there is no help message

    uint32_t startCol;
    uint32_t endCol;
    uint32_t lineNumber;
    
} ErrorMessage;

void printErrorMessage(ErrorMessage errMsg);
ErrorMessage createError(char* fileName, char* sourceCode, char* errType, char* message, char* help, uint32_t lineNumber, uint32_t colStart, uint32_t colEnd);
ErrorMessage createWarning(char* fileName, char* sourceCode, char* errType, char* message, char* help, uint32_t lineNumber, uint32_t colStart, uint32_t colEnd);

#endif