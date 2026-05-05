#include "token.h"

char* tokenTypeToCStr(CometTokenType tokType) {
    switch (tokType) {
        case CT_IDENT:
            return "CT_IDENT";
        case CT_KEYWORD:
            return "CT_KEYWORD";
        case CT_INT_LITERAL:
            return "CT_INT_LITERAL";
        case CT_FLOAT_LITERAL:
            return "CT_FLOAT_LITERAL";
        case CT_BOOL_LITERAL:
            return "CT_BOOL_LITERAL";
        case CT_STRING_LITERAL:
            return "CT_STRING_LITERAL";
        case CT_EQ:
            return "CT_EQ";
        case CT_OPEN_CURLY:
            return "CT_OPEN_CURLY";
        case CT_CLOSE_CURLY:
            return "CT_CLOSE_CURLY";
        case CT_OPEN_PAREN:
            return "CT_OPEN_PAREN";
        case CT_CLOSE_PAREN:
            return "CT_CLOSE_PAREN";
        case CT_LT:
            return "CT_LT";
        case CT_GT:
            return "CT_GT";
        case CT_LTE:
            return "CT_LTE";
        case CT_GTE:
            return "CT_GTE";
        case CT_EQ_EQ:
            return "CT_EQ_EQ";
        case CT_DOT:
            return "CT_DOT";
        case CT_COLON:
            return "CT_DOT";
        case CT_DOT_DOT:
            return "CT_DOT_DOT";
        case CT_END_LABEL:
            return "CT_END_LABEL";
        case CT_PLUS:
            return "CT_PLUS";
        case CT_MINUS:
            return "CT_MINUS";
        case CT_TIMES:
            return "CT_TIMES";
        case CT_DIVIDE:
            return "CT_DIVIDE";
        case CT_MOD:
            return "CT_MOD";
        case CT_POW:
            return "CT_POW";
        case CT_EOF:
            return "CT_EOF";
        case CT_ARROW:
            return "CT_ARROW";
        case CT_INLINE_FUNC_ARROW:
            return "CT_INLINE_FUNC_ARROW";
        case CT_COMMA:
            return "CT_COMMA";
        case CT_NOT:
            return "CT_NOT";
        case CT_NOT_EQ:
            return "CT_NOT_EQ";
        default:
            return "FIXME";
        
    }
}

char* tokenToCStr(CometToken tok) {
    char* buffer = malloc(256);

    switch (tok.literalType) {
        case CL_STRING:
            sprintf(buffer, "<Token literal=\"%s\", type=%s>", tok.value.literal, tokenTypeToCStr(tok.type));
            return buffer;
        case CL_INT:
            sprintf(buffer, "<Token literal=%lld, type=%s>", tok.value.intVal, tokenTypeToCStr(tok.type));
            return buffer;
        case CL_DOUBLE:
            sprintf(buffer, "<Token literal=%f, type=%s>", tok.value.doubleVal, tokenTypeToCStr(tok.type));
            return buffer;
        case CL_BOOL:
            if (tok.value.boolVal)  
                sprintf(buffer, "<Token literal=true, type=%s>", tokenTypeToCStr(tok.type));
            else
                sprintf(buffer, "<Token literal=false, type=%s>", tokenTypeToCStr(tok.type));

            return buffer;
        default:
            return "<Token type=FIXME>";
    }
}