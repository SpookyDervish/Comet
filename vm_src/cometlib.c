#include "../include/cometlib.h"
#include <__stdarg_va_list.h>
#include <stdarg.h>
#include <stdint.h>

CometOperand cometValue(CometValueTypeKind valueType, ...) {
    va_list args;
    va_start(args, valueType);

    CometOperand newVal;

    switch (valueType) {
        case COMET_SMALL: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.smallVal = (int8_t)va_arg(args, int8_t)
            };
            break;
        case COMET_INT: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.intVal = (int32_t)va_arg(args, int32_t)
            };
            break;
        case COMET_BIG: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.bigVal = (int64_t)va_arg(args, int64_t)
            };
            break;
        case COMET_FLOAT: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.floatVal = (float)va_arg(args, float)
            };
            break;
        case COMET_DOUBLE: 
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.doubleVal = (double)va_arg(args, double)
            };
            break;
        case COMET_BOOL:
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.boolVal = (bool)va_arg(args, bool)
            };
            break;
        case COMET_STRUCT:
            newVal = (CometOperand){
                .type = CO_IMMEDIATE,
                .imm.objectVal = (CometObject*)va_arg(args, CometObject*)
            };
            break;
        case COMET_FUNCTION:
            newVal = (CometOperand){
                .type = CO_SYMBOL,
                .symbolIdx = va_arg(args, uint32_t)
            };
            break;
        case COMET_VOID: 
            newVal = (CometOperand){
                .type = CO_NONE
            };
            break;
    }

    return newVal;
}