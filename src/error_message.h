#ifndef ERROR_MESSAGE_H
#define ERROR_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../include/list.h"

typedef struct {
    uint32_t colStart;
    uint32_t colEnd;
    char* style;
    char* message;
} ErrorRegion;

UseList(ErrorRegion);

typedef struct {
    char* message;  // "Unexpected token '#'"
    char* errType;  // "SyntaxError", "TypeError", etc...
    bool isWarning; // true or false
    char* help;     // can be NULL if there is no help message

    List(ErrorRegion) regions;

    uint32_t lineNumber;
    
} ErrorMessage;

void printErrorMessage(ErrorMessage errMsg, char* sourceCode);
ErrorMessage createError(char* errType, char* message, char* help, uint32_t lineNumber);
ErrorMessage createWarning(char* errType, char* message, char* help, uint32_t lineNumber);
void destroyError(ErrorMessage errMsg);
void createErrorRegion(ErrorMessage* errMsg, char* message, char* style, uint32_t colStart, uint32_t colEnd);


#endif