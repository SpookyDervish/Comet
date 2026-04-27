#include "lexer.h"
#include "token.h"
#include <stdio.h>

int main() {
    ResultType(CometLexer, charptr) lexer = newLexer("string x = \"hello world!\"");
    ResultType(tokenList, charptr) tokens = lex(&lexer.as.success);

    if (tokens.error) {
        printf("lexer error: %s\n", tokens.as.error);
    }



    for (size_t i = 0; i < tokens.as.success.count; i++) {
        CometToken* tok = get(tokens.as.success, i);
        printf("%s\n", tokenToCStr(*tok));
    }
}