#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef ESTR_H
#define ESTR_H

/*

    estr.h - Easy string manipulation
    This library has macros to allow easier manipulation of strings. No longer shall
    you have to malloc and realloc away to keep adding to your strings.

    Usage:

    Estr myString = CREATE_ESTR("my awesome string");
    APPEND_ESTR(myString, " is so cool");
    printf("%s\n", myString.str);

*/

#define CREATE_ESTR(instr) \
    (Estr) { \
        .str = instr,\
        .size = strlen(instr),\
        .shouldBeFreed = 0, \
        .destroyed = 0 \
    }

#define APPEND_ESTR(estr, instr) { \
    estr.size = estr.size + strlen(instr); \
    char* tmp_ptr = malloc(estr.size + 1); \
    if (tmp_ptr == NULL) printf("WARNING: Could not realloc estr " #estr "\n"); \
    else { \
        snprintf(tmp_ptr, estr.size + 1, "%s%s", estr.str, instr); \
        if (estr.shouldBeFreed > 0) free(estr.str); \
        estr.shouldBeFreed = 1; \
        estr.str = tmp_ptr; \
    } \
}

#define DESTROY_ESTR(estr) if (estr.shouldBeFreed > 0 && estr.destroyed < 1) free(estr.str); 

typedef struct Estr {
    char* str;
    size_t size;
    int8_t shouldBeFreed;
    int8_t destroyed;
} Estr; 

#endif // ESTR_H