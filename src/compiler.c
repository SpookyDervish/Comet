#include "compiler.h"

// -- STATEMENTS -- //
ResultType(Nothing, charptr) visitProgram(CometCompiler* compiler, CometASTNode* node) {
    for (size_t i = 0; i < node->data.AST_PROGRAM.numStatements; i++) {
        ResultType(Nothing, charptr) result = compile(compiler, node->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;
    }

    return Success(Nothing, charptr, {});
}

// -- MAIN --//
ResultType(Nothing, charptr) compile(CometCompiler* compiler, CometASTNode* node) {

    switch (node->nodeType) {
        case AST_PROGRAM:
            return visitProgram(compiler, node);

        default: {
            char* buffer = malloc(128);
            sprintf(buffer, "No visit method for %s node.", ASTNodeTypeToCStr(node->nodeType));
            return Error(Nothing, charptr, buffer);
        }
    }

    return Success(Nothing, charptr, {});
}