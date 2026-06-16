#include "error_message.h"
#include "../lib/ansi.h"
#include "util.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>

void printErrorMessage(ErrorMessage errMsg) {
    char* errorLineSource = getLineInString(errMsg.sourceCode, errMsg.lineNumber);
    bool lineSourceOwned = true;
    if (errorLineSource == NULL) {
        errorLineSource = "??? (something REALLLYYY bad has happened)";
        lineSourceOwned = false;
    }

    static const unsigned int linesToShow = 2; // 2 above, 2 below

    if (!errMsg.isWarning) {
        // Error: SyntaxError
        fprintf(stderr, ESC_BOLD "Error: " ESC_RED_FG ESC_UNDERLINE "%s\n" ESC_RESET, errMsg.errType);

        //    × Unexpected token "#"
        fprintf(stderr, ESC_BOLD ESC_RED_FG "   ×" ESC_RESET " %s\n", errMsg.message);
    } else {
        fprintf(stderr, ESC_BOLD "Warning: " ESC_YELLOW_FG ESC_UNDERLINE "%s\n" ESC_RESET, errMsg.errType);

        fprintf(stderr, ESC_BOLD ESC_YELLOW_FG "   ǃ" ESC_RESET " %s\n", errMsg.message);
    }

    fprintf(
        stderr,
        "\n    ╭─ " ESC_BRIGHT_BLUE_FG ESC_BOLD "%s" ESC_RESET " [line %d, column %d:%d]\n",
        errMsg.fileName,
        errMsg.lineNumber,
        errMsg.startCol,
        errMsg.endCol
    );

    int startLine = errMsg.lineNumber - linesToShow;
    if (startLine < 1)
        startLine = 1;

    for (unsigned int lineNumber = startLine; lineNumber < errMsg.lineNumber + linesToShow; lineNumber++) {
        char* line = getLineInString(errMsg.sourceCode, lineNumber);
        if (line == NULL)
            break;

        fprintf(stderr, "    │  " ESC_DIM "%d" ESC_RESET "  %s\n", lineNumber, line);
        if (lineNumber == errMsg.lineNumber) {
            fprintf(stderr, "    ┊     ");
            
            char* padding = repeatString(" ", errMsg.startCol-1);
            int underlineLen = errMsg.endCol - errMsg.startCol + 1;
            char underline[underlineLen+1] = {};
            underline[0] = '^';

            char* middle = repeatString("-", underlineLen-1);
            strcat(underline, middle);

            underline[underlineLen] = 0;

            if (!errMsg.isWarning)
                fprintf(stderr, ESC_BOLD ESC_RED_FG "%s%s\n" ESC_RESET, padding, underline);
            else
                fprintf(stderr, ESC_BOLD ESC_YELLOW_FG "%s%s\n" ESC_RESET, padding, underline);

        }
    } 
    

    if (errMsg.help != NULL) {
        fprintf(stderr, "    ╰────❯ " ESC_CYAN_FG ESC_DIM "help" ESC_RESET ": %s\n", errMsg.help);
    } else {
        fprintf(stderr, "    ╰────<\n");
    }

}

ErrorMessage createError(char* fileName, char* sourceCode, char* errType, char* message, char* help, uint32_t lineNumber, uint32_t startCol, uint32_t endCol) {
    return (ErrorMessage){
        fileName,
        sourceCode,
        message,
        errType,
        false,
        help,
        startCol,
        endCol,
        lineNumber
    };
}

ErrorMessage createWarning(char* fileName, char* sourceCode, char* errType, char* message, char* help, uint32_t lineNumber, uint32_t startCol, uint32_t endCol) {
    return (ErrorMessage){
        fileName,
        sourceCode,
        message,
        errType,
        true,
        help,
        startCol,
        endCol,
        lineNumber
    };
}