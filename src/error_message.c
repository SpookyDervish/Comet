#include "error_message.h"
#include "../lib/ansi.h"
#include "util.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

ErrorRegion getRegion(ErrorMessage errMsg, int64_t i) {
    if (i < 0)
        i = errMsg.regions.count + i;

    return *get(errMsg.regions, i);
}

void printErrorMessage(ErrorMessage errMsg, char* sourceCode) {
    char* errorLineSource = getLineInString(sourceCode, errMsg.lineNumber);
    if (errorLineSource == NULL)
        errorLineSource = "??? (something REALLLYYY bad has happened)";
    

    // Error: SyntaxError
    fprintf(stderr, ESC_BOLD "Error: " ESC_RED_FG ESC_UNDERLINE "%s\n" ESC_RESET, errMsg.errType);

    //    × Unexpected token "#"
    fprintf(stderr, ESC_BOLD ESC_RED_FG "   ×" ESC_RESET " %s\n", errMsg.message);
    //     ╭─[line 1, column 1:1]
    fprintf(stderr, "    ╭─[line %d, column %d:%d]\n", errMsg.lineNumber, getRegion(errMsg, 0).colStart, getRegion(errMsg, -1).colEnd);
    //   1 │ # hello 
    fprintf(stderr, ESC_DIM "  %d " ESC_RESET "│ %s\n", errMsg.lineNumber, errorLineSource);
    //     . ┰ 
    for (size_t i = 0; i < errMsg.regions.count; i++) {
        
    }

    //     . ╰─ here
    //     ╰─

}

ErrorMessage createError(char* errType, char* message, char* help, uint32_t lineNumber) {
    return (ErrorMessage){
        message,
        errType,
        false,
        help,
        newList(ErrorRegion),
        lineNumber
    };
}

ErrorMessage createWarning(char* errType, char* message, char* help, uint32_t lineNumber) {
    return (ErrorMessage){
        message,
        errType,
        true,
        help,
        newList(ErrorRegion),
        lineNumber
    };
}

void destroyError(ErrorMessage errMsg) {
    destroy(errMsg.regions);
}

void createErrorRegion(ErrorMessage* errMsg, char* message, char* style, uint32_t colStart, uint32_t colEnd) {
    ErrorRegion newRegion = {
        colStart,
        colEnd,
        style,
        message
    };


    append(errMsg->regions, newRegion);
}