#include "ast.h"

CometASTNode* allocateNode(CometASTNode node) {
    CometASTNode* ptr = malloc(sizeof(CometASTNode));
    if (ptr) *ptr = node;
    return ptr;
}

void freeNode(CometASTNode* node) {
    if (node == NULL) return;

    switch (node->nodeType) {
        case AST_PROGRAM: {
            for (size_t i = 0; i < node->data.AST_PROGRAM.numStatements; i++) {
                freeNode(node->data.AST_PROGRAM.statements[i]);
            }
            free(node->data.AST_PROGRAM.statements);
            break;
        }
        case AST_FUNC_DEF_STATEMENT: {
            for (size_t i = 0; i < node->data.AST_FUNC_DEF_STATEMENT.args.count; i++) {
                freeNode(*get(node->data.AST_FUNC_DEF_STATEMENT.args, i));
            }
            destroy(node->data.AST_FUNC_DEF_STATEMENT.args);
            freeNode(node->data.AST_FUNC_DEF_STATEMENT.program);
            freeNode(node->data.AST_FUNC_DEF_STATEMENT.ident);
            freeNode(node->data.AST_FUNC_DEF_STATEMENT.inlineExpr);
            freeNode(node->data.AST_FUNC_DEF_STATEMENT.returnType);
            break;
        }
        case AST_INT: break;
        case AST_DOUBLE: break;
        case AST_BOOL: break;
        case AST_IDENTIFIER: {
            free(node->data.AST_IDENTIFIER.ident);
            break;
        }
        case AST_STRING: {
            free(node->data.AST_STRING.value);
            break;
        }
        case AST_WHILE_STATEMENT: {
            freeNode(node->data.AST_WHILE_STATEMENT.expression);
            freeNode(node->data.AST_WHILE_STATEMENT.program);
            break;
        }
        case AST_FUNC_CALL: {
            for (size_t i = 0; i < node->data.AST_FUNC_CALL.args.count; i++) {
                freeNode(*get(node->data.AST_FUNC_CALL.args, i));
            }

            destroy(node->data.AST_FUNC_CALL.args);
            freeNode(node->data.AST_FUNC_CALL.ident);
            break;
        }
        case AST_CONSTRUCTOR_DEF: {
            for (size_t i = 0; i < node->data.AST_CONSTRUCTOR_DEF.args.count; i++) {
                freeNode(*get(node->data.AST_CONSTRUCTOR_DEF.args, i));
            }

            destroy(node->data.AST_CONSTRUCTOR_DEF.args);
            freeNode(node->data.AST_CONSTRUCTOR_DEF.program);
            break;
        }
        case AST_STRUCT_DEF_STATEMENT: {
            for (size_t i = 0; i < node->data.AST_STRUCT_DEF_STATEMENT.fieldDefs.count; i++) {
                freeNode(*get(node->data.AST_STRUCT_DEF_STATEMENT.fieldDefs, i));
            }
            destroy(node->data.AST_STRUCT_DEF_STATEMENT.fieldDefs);

            freeNode(node->data.AST_STRUCT_DEF_STATEMENT.ident);
            freeNode(node->data.AST_STRUCT_DEF_STATEMENT.constructor);
            freeNode(node->data.AST_STRUCT_DEF_STATEMENT.destructor);
            freeNode(node->data.AST_STRUCT_DEF_STATEMENT.parentName);
            break;
        }
        case AST_ARG_DEF: {
            freeNode(node->data.AST_ARG_DEF.type);
            freeNode(node->data.AST_ARG_DEF.ident);
            break;
        }
        case AST_INFIX_EXPRESSION: {
            freeNode(node->data.AST_INFIX_EXPRESSION.left);
            freeNode(node->data.AST_INFIX_EXPRESSION.right);
            break;
        }
        case AST_PREFIX_EXPRESSION: {
            freeNode(node->data.AST_PREFIX_EXPRESSION.right);
            break;
        }
        case AST_EXPRESSION_STATEMENT: {
            freeNode(node->data.AST_EXPRESSION_STATEMENT.expression);
            break;
        }
        case AST_ASSIGN_STATEMENT: {
            freeNode(node->data.AST_ASSIGN_STATEMENT.expression);
            freeNode(node->data.AST_ASSIGN_STATEMENT.ident);
            freeNode(node->data.AST_ASSIGN_STATEMENT.type);
            break;
        }
        case AST_REASSIGN_STATEMENT: {
            freeNode(node->data.AST_REASSIGN_STATEMENT.expression);
            freeNode(node->data.AST_REASSIGN_STATEMENT.ident);
            break;
        }
        case AST_NEW_STATEMENT: {
            for (size_t i = 0; i < node->data.AST_NEW_STATEMENT.args.count; i++) {
                freeNode(*get(node->data.AST_NEW_STATEMENT.args, i));
            }
            destroy(node->data.AST_NEW_STATEMENT.args);
            freeNode(node->data.AST_NEW_STATEMENT.structName);
            break;
        }
        case AST_RETURN_STATEMENT: {
            freeNode(node->data.AST_RETURN_STATEMENT.expression);
            break;
        }
        case AST_FOR_STATEMENT: {
            freeNode(node->data.AST_FOR_STATEMENT.start);
            freeNode(node->data.AST_FOR_STATEMENT.end);
            freeNode(node->data.AST_FOR_STATEMENT.step);
            freeNode(node->data.AST_FOR_STATEMENT.ident);
            freeNode(node->data.AST_FOR_STATEMENT.program);
            freeNode(node->data.AST_FOR_STATEMENT.type);
            break;
        }

        default: {
            printf("WARNING: Unhandled AST node type in freeNode: %s\n", ASTNodeTypeToCStr(node->nodeType));
            break;
        }
    }

    free(node);
}

char* ASTNodeTypeToCStr(CometASTNodeType nodeType) {
    switch (nodeType) {
        case AST_INT:
            return "AST_INT";
        case AST_DOUBLE:
            return "AST_DOUBLE";
        case AST_STRING:
            return "AST_STRING";
        case AST_IDENTIFIER:
            return "AST_IDENTIFIER";
        case AST_BOOL:
            return "AST_BOOL";

        case AST_PROGRAM:
            return "AST_PROGRAM";

        case AST_EXPRESSION_STATEMENT:
            return "AST_EXPRESSION_STATEMENT";
        case AST_ASSIGN_STATEMENT:
            return "AST_ASSIGN_STATEMENT";
        case AST_REASSIGN_STATEMENT:
            return "AST_REASSIGN_STATEMENT";
        case AST_RETURN_STATEMENT:
            return "AST_RETURN_STATEMENT";
        case AST_IF_STATEMENT:
            return "AST_IF_STATEMENT";
        case AST_WHILE_STATEMENT:
            return "AST_WHILE_STATEMENT";
        case AST_FOR_STATEMENT:
            return "AST_FOR_STATEMENT";
        case AST_CONSTRUCTOR_DEF:
            return "AST_CONSTRUCTOR_DEF";
        case AST_STRUCT_DEF_STATEMENT:
            return "AST_STRUCT_DEF_STATEMENT";
        case AST_FUNC_DEF_STATEMENT:
            return "AST_FUNC_DEF_STATEMENT";
        case AST_OVERRIDE_STATEMENT:
            return "AST_OVERRIDE_STATEMENT";
        case AST_IMPORT_STATEMENT:
            return "AST_IMPORT_STATEMENT";

        case AST_INFIX_EXPRESSION:
            return "AST_INFIX_EXPRESSION";
        case AST_PREFIX_EXPRESSION:
            return "AST_PREFIX_EXPRESSION";
        case AST_FUNC_CALL:
            return "AST_FUNC_CALL";
        case AST_ARG_DEF:
            return "AST_ARG_DEF";
        case AST_NEW_STATEMENT:
            return "AST_NEW_STATEMENT";

        default:
            return "AST_UNKOWN (FIXME)";
    }
}