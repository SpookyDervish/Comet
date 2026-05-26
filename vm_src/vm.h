#ifndef VM_H
#define VM_H

#include "operand.h"
#include <stdint.h>

typedef struct {
    CometOperand constants;
    int64_t stack[];
} CometVM;

#endif