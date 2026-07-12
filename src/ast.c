#include "ast.h"

CometASTNode* allocateNode(CometASTNode node) {
    CometASTNode* ptr = malloc(sizeof(CometASTNode));
    if (ptr) *ptr = node;
    return ptr;
}

bool nodeIsALiteral(CometASTNode* node) {
    if (!node) return false;

    switch (node->nodeType) {
        case AST_INT:
        case AST_BOOL:
        case AST_DOUBLE:
        case AST_STRING:
        case AST_CHAR:
            return true;
        
        default:
            return false;
    }
}

uint64_t getNodeIntValue(CometASTNode* node) {
    return node->data.AST_INT.number;
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
        case AST_CHAR: break;
        
        case AST_WHILE_STATEMENT: {
            freeNode(node->data.AST_WHILE_STATEMENT.expression);
            freeNode(node->data.AST_WHILE_STATEMENT.program);
            break;
        }
        case AST_IF_STATEMENT: {
            freeNode(node->data.AST_IF_STATEMENT.program);
            freeNode(node->data.AST_IF_STATEMENT.expression);
            freeNode(node->data.AST_IF_STATEMENT.elseProgram);
            break;
        }
        case AST_DROP_STATEMENT: {
            freeNode(node->data.AST_DROP_STATEMENT.value);
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
        case AST_DESTRUCTOR_DEF: {
            freeNode(node->data.AST_DESTRUCTOR_DEF.program);
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
        case AST_OVERRIDE_STATEMENT: {
            freeNode(node->data.AST_OVERRIDE_STATEMENT.funcDef);
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
        case AST_BREAKPOINT_STATEMENT: break;
        case AST_IMPORT_STATEMENT: {
            for (size_t i = 0; i < node->data.AST_IMPORT_STATEMENT.importChain.count; i++) {
                freeNode(*get(node->data.AST_IMPORT_STATEMENT.importChain, i));
            }
            break;
        }   

        case AST_CONTINUE_STATEMENT:
        case AST_BREAK_STATEMENT:
            break;

        case AST_TYPE: {
            for (size_t i = 0; i < node->data.AST_TYPE.baseType.count; i++) {
                freeNode(*get(node->data.AST_TYPE.baseType, i));
            }
            destroy(node->data.AST_TYPE.baseType);
            for (size_t i = 0; i < node->data.AST_TYPE.shape.count; i++) {
                freeNode(*get(node->data.AST_TYPE.shape, i));
            }
            destroy(node->data.AST_TYPE.shape);
            break;
        }

        case AST_ARRAY: {
            for (size_t i = 0; i < node->data.AST_ARRAY.elements.count; i++) {
                freeNode(*get(node->data.AST_ARRAY.elements, i));
            }
            break;
        }

        case AST_TRY_STATEMENT: {
            freeNode(node->data.AST_TRY_STATEMENT.tryBlock);
            freeNode(node->data.AST_TRY_STATEMENT.exceptBlock);
            break;
        }

        case AST_THROW_STATEMENT: {
            freeNode(node->data.AST_THROW_STATEMENT.newStmt);
            break;
        }

        case AST_ENUM_DEF: {
            freeNode(node->data.AST_ENUM_DEF.ident);

            for (size_t i = 0; i < node->data.AST_ENUM_DEF.items.count; i++) {
                freeNode(*get(node->data.AST_ENUM_DEF.items, i));
            }

            destroy(node->data.AST_ENUM_DEF.items);
            break;
        }

        case AST_AS_FUNC_DEF: {
            freeNode(node->data.AST_AS_FUNC_DEF.type);
            freeNode(node->data.AST_AS_FUNC_DEF.body);
            break;
        }

        case AST_AS_EXPR: {
            free(node->data.AST_AS_EXPR.left);
            free(node->data.AST_AS_EXPR.type);
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
        case AST_ARRAY:
            return "AST_ARRAY";

        case AST_TYPE:
            return "AST_TYPE";

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
        case AST_DESTRUCTOR_DEF:
            return "AST_DESTRUCTOR_DEF";
        case AST_STRUCT_DEF_STATEMENT:
            return "AST_STRUCT_DEF_STATEMENT";
        case AST_FUNC_DEF_STATEMENT:
            return "AST_FUNC_DEF_STATEMENT";
        case AST_OVERRIDE_STATEMENT:
            return "AST_OVERRIDE_STATEMENT";
        case AST_IMPORT_STATEMENT:
            return "AST_IMPORT_STATEMENT";
        case AST_BREAKPOINT_STATEMENT:
            return "AST_BREAKPOINT_STATEMENT";
        case AST_TRY_STATEMENT:
            return "AST_TRY_STATEMENT";
        case AST_THROW_STATEMENT:
            return "AST_THROW_STATEMENT";
        case AST_AS_FUNC_DEF:
            return "AST_AS_FUNC_DEF";
        case AST_CONTINUE_STATEMENT:
            return "AST_CONTINUE_STATEMENT";
        case AST_BREAK_STATEMENT:
            return "AST_BREAK_STATEMENT";

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
        case AST_ENUM_DEF:
            return "AST_ENUM_DEF";

        default:
            return "AST_UNKOWN (FIXME)";
    }
}

void appendNodeToBuff(CometASTNode* node, StringBuffer* buff) {
    if (!node) return;

    switch (node->nodeType) {
        case AST_PROGRAM:
            sbAppend(buff, "Program:\n");

            CometASTNode** statements = node->data.AST_PROGRAM.statements;
            

            for (size_t i = 0; i < node->data.AST_PROGRAM.numStatements; i++) {
                sbAppend(buff, "    %ld. ", i+1);
                appendNodeToBuff(statements[i], buff);
                sbAppend(buff, "\n");
            }
            break;

        case AST_INT: sbAppend(buff, "%ld", node->data.AST_INT.number); break;
        case AST_BOOL: sbAppend(buff, "%s", node->data.AST_BOOL.value ? "true" : "false"); break;
        case AST_DOUBLE: sbAppend(buff, "%f", node->data.AST_DOUBLE.number); break;
        case AST_STRING: sbAppend(buff, "\"%s\"", node->data.AST_STRING.value); break;
        case AST_CHAR: sbAppend(buff, "'%c'", node->data.AST_CHAR.value); break;
        case AST_IDENTIFIER: sbAppend(buff, "%s", node->data.AST_IDENTIFIER.ident); break;
        case AST_ARRAY:
            sbAppend(buff, "[");

            for (size_t i = 0; i < node->data.AST_ARRAY.elements.count; i++) {
                appendNodeToBuff(*get(node->data.AST_ARRAY.elements, i), buff);

                if (i < node->data.AST_ARRAY.elements.count-1) {
                    sbAppend(buff, ", ");
                }
            }

            sbAppend(buff, "]");
            break;
            
        case AST_TYPE: {
            for (size_t i = 0; i < node->data.AST_TYPE.baseType.count; i++) {
                appendNodeToBuff(*get(node->data.AST_TYPE.baseType, i), buff);
                if (i < node->data.AST_TYPE.baseType.count - 1)
                    sbAppend(buff, ".");
            }

            nodeList genericTypes = node->data.AST_TYPE.genericTypes;
            if (genericTypes.count > 0) {
                sbAppend(buff, "<");

                for (size_t i = 0; i < genericTypes.count; i++) {
                    appendNodeToBuff(*get(genericTypes, i), buff);
                    if (i < genericTypes.count - 1) {
                        sbAppend(buff, ", ");
                    } else {
                        sbAppend(buff, ">");
                    }
                }
            }

            if (node->data.AST_TYPE.dimensions > 0) {
                sbAppend(buff, "[");
                for (size_t i = 0; i < node->data.AST_TYPE.shape.count; i++) {
                    CometASTNode* size = *get(node->data.AST_TYPE.shape, i);
                    appendNodeToBuff(size, buff);

                    if (i < node->data.AST_TYPE.shape.count - 1) {
                        sbAppend(buff, ", ");
                    } else {
                        sbAppend(buff, "]");
                    }
                }
            }

            break;
        }

        case AST_INFIX_EXPRESSION:
            sbAppend(buff, "(");
            appendNodeToBuff(node->data.AST_INFIX_EXPRESSION.left, buff);
            sbAppend(buff, " %s ", node->data.AST_INFIX_EXPRESSION.op.value.literal);
            appendNodeToBuff(node->data.AST_INFIX_EXPRESSION.right, buff);
            sbAppend(buff, ")");
            break;

        case AST_PREFIX_EXPRESSION:
            sbAppend(buff, "%s(", node->data.AST_PREFIX_EXPRESSION.op.value.literal);
            appendNodeToBuff(node->data.AST_PREFIX_EXPRESSION.right, buff);
            sbAppend(buff, ")");
            break;

        case AST_FUNC_CALL:
            appendNodeToBuff(node->data.AST_FUNC_CALL.ident, buff);
            sbAppend(buff, "(");
            for (size_t i = 0; i < node->data.AST_FUNC_CALL.args.count; i++) {
                CometASTNode* arg = *get(node->data.AST_FUNC_CALL.args, i);
                appendNodeToBuff(arg, buff);

                if (i < node->data.AST_FUNC_CALL.args.count-1)
                    sbAppend(buff, ", ");
            }
            sbAppend(buff, ")");
            break;

        case AST_EXPRESSION_STATEMENT:
            appendNodeToBuff(node->data.AST_EXPRESSION_STATEMENT.expression, buff);
            break;
        case AST_ASSIGN_STATEMENT: {
            FieldAttribute attrib = node->data.AST_ASSIGN_STATEMENT.attrib;
            if (attrib != FIELD_PUBLIC) {
                switch (attrib) {
                    case FIELD_PRIVATE: sbAppend(buff, "private "); break;
                    case FIELD_PROTECTED: sbAppend(buff, "protected "); break;
                    case FIELD_READ_ONLY: sbAppend(buff, "readonly "); break;
                    default: break;
                }
            }

            appendNodeToBuff(node->data.AST_ASSIGN_STATEMENT.type, buff);
            sbAppend(buff, " ");
            appendNodeToBuff(node->data.AST_ASSIGN_STATEMENT.ident, buff);

            CometASTNode* value = node->data.AST_ASSIGN_STATEMENT.expression;

            if (value) {
                sbAppend(buff, " = ");
                appendNodeToBuff(value, buff);
            }
            break;
        }
        case AST_REASSIGN_STATEMENT:
            appendNodeToBuff(node->data.AST_REASSIGN_STATEMENT.ident, buff);
            sbAppend(buff, " %s ", node->data.AST_REASSIGN_STATEMENT.op.value.literal);
            appendNodeToBuff(node->data.AST_REASSIGN_STATEMENT.expression, buff);
            break;
        case AST_WHILE_STATEMENT:
            sbAppend(buff, "while ");
            appendNodeToBuff(node->data.AST_WHILE_STATEMENT.expression, buff);
            sbAppend(buff, " {\n");
            appendNodeToBuff(node->data.AST_WHILE_STATEMENT.program, buff);
            sbAppend(buff, "       }");
            break;
        case AST_BREAK_STATEMENT:
            sbAppend(buff, "break");
            break;
        case AST_CONTINUE_STATEMENT:
            sbAppend(buff, "continue");
            break;
        case AST_ARG_DEF:
            appendNodeToBuff(node->data.AST_ARG_DEF.type, buff);
            sbAppend(buff, " ");
            appendNodeToBuff(node->data.AST_ARG_DEF.ident, buff);
            break;
        case AST_FUNC_DEF_STATEMENT:
            sbAppend(buff, "func ");
            appendNodeToBuff(node->data.AST_FUNC_DEF_STATEMENT.ident, buff);
            sbAppend(buff, "(");

            for (size_t i = 0; i < node->data.AST_FUNC_DEF_STATEMENT.args.count; i++) {
                CometASTNode* arg = *get(node->data.AST_FUNC_DEF_STATEMENT.args, i);
                appendNodeToBuff(arg, buff);

                if (i < node->data.AST_FUNC_DEF_STATEMENT.args.count-1)
                    sbAppend(buff, ", ");
            }
            
            sbAppend(buff, ") -> ");
            appendNodeToBuff(node->data.AST_FUNC_DEF_STATEMENT.returnType, buff);
            sbAppend(buff, " ");

            if (node->data.AST_FUNC_DEF_STATEMENT.isInline) {
                sbAppend(buff, "=> ");
                appendNodeToBuff(node->data.AST_FUNC_DEF_STATEMENT.inlineExpr, buff);
            } else {
                sbAppend(buff, "{\n");
                appendNodeToBuff(node->data.AST_FUNC_DEF_STATEMENT.program, buff);
                sbAppend(buff, "       }");
            }

            break;
        case AST_RETURN_STATEMENT:
            sbAppend(buff, "return ");
            if (node->data.AST_RETURN_STATEMENT.expression)
                appendNodeToBuff(node->data.AST_RETURN_STATEMENT.expression, buff);
            break;
        case AST_IF_STATEMENT:
            sbAppend(buff, "if ");
            appendNodeToBuff(node->data.AST_IF_STATEMENT.expression, buff);
            sbAppend(buff, " {\n");
            appendNodeToBuff(node->data.AST_IF_STATEMENT.program, buff);
            sbAppend(buff, "       } ");

            if (node->data.AST_IF_STATEMENT.elseProgram) {
                sbAppend(buff, "else {\n");
                appendNodeToBuff(node->data.AST_IF_STATEMENT.elseProgram, buff);
                sbAppend(buff, "       } ");
            }
            break;
        case AST_FOR_STATEMENT:
            sbAppend(buff, "for ");
            appendNodeToBuff(node->data.AST_FOR_STATEMENT.type, buff);
            sbAppend(buff, " ");
            appendNodeToBuff(node->data.AST_FOR_STATEMENT.ident, buff);
            sbAppend(buff, " in ");

            if (node->data.AST_FOR_STATEMENT.array) {
                appendNodeToBuff(node->data.AST_FOR_STATEMENT.array, buff);
            } else {
                appendNodeToBuff(node->data.AST_FOR_STATEMENT.start, buff);
                sbAppend(buff, "..");
                appendNodeToBuff(node->data.AST_FOR_STATEMENT.end, buff);
                sbAppend(buff, " step ");
                appendNodeToBuff(node->data.AST_FOR_STATEMENT.step, buff);
            }

            
            sbAppend(buff, " {\n");
            appendNodeToBuff(node->data.AST_FOR_STATEMENT.program, buff);
            sbAppend(buff, "       }");
            break;

        case AST_STRUCT_DEF_STATEMENT:
            sbAppend(buff, "struct ");
            appendNodeToBuff(node->data.AST_STRUCT_DEF_STATEMENT.ident, buff);

            if (node->data.AST_STRUCT_DEF_STATEMENT.parentName) {
                sbAppend(buff, " : %s", node->data.AST_STRUCT_DEF_STATEMENT.parentName->data.AST_IDENTIFIER.ident);
            }

            List(astNodePtr) genericTypes = node->data.AST_STRUCT_DEF_STATEMENT.genericTypes;
            if (genericTypes.count > 0) {
                sbAppend(buff, " <");
                for (size_t i = 0; i < genericTypes.count; i++) {
                    appendNodeToBuff(*get(genericTypes, i), buff);
                    if (i < genericTypes.count - 1) {
                        sbAppend(buff, ", ");
                    } else {
                        sbAppend(buff, ">");
                    }
                }
            }

            sbAppend(buff, " {\n");

            List(astNodePtr) structDefs = node->data.AST_STRUCT_DEF_STATEMENT.fieldDefs;

            for (size_t i = 0; i < structDefs.count; i++) {
                sbAppend(buff, "           ");
                appendNodeToBuff(*get(structDefs, i), buff);
                sbAppend(buff, "\n");
            }

            
            if (node->data.AST_STRUCT_DEF_STATEMENT.constructor) {
                sbAppend(buff, "\n");
                appendNodeToBuff(node->data.AST_STRUCT_DEF_STATEMENT.constructor, buff);
                sbAppend(buff, "\n");
            }

            sbAppend(buff, "       }");
            break;
        case AST_CONSTRUCTOR_DEF: 
            sbAppend(buff, "           init(");
            for (size_t i = 0; i < node->data.AST_CONSTRUCTOR_DEF.args.count; i++) {
                CometASTNode* arg = *get(node->data.AST_CONSTRUCTOR_DEF.args, i);
                appendNodeToBuff(arg, buff);

                if (i < node->data.AST_CONSTRUCTOR_DEF.args.count-1)
                    sbAppend(buff, ", ");
            }
            sbAppend(buff, ") {\n");
            appendNodeToBuff(node->data.AST_CONSTRUCTOR_DEF.program, buff);
            sbAppend(buff, "           }");

            break;
        case AST_NEW_STATEMENT:
            sbAppend(buff, "new ");
            appendNodeToBuff(node->data.AST_NEW_STATEMENT.structName, buff);
            sbAppend(buff, "(");
            for (size_t i = 0; i < node->data.AST_CONSTRUCTOR_DEF.args.count; i++) {
                CometASTNode* arg = *get(node->data.AST_CONSTRUCTOR_DEF.args, i);
                appendNodeToBuff(arg, buff);

                if (i < node->data.AST_CONSTRUCTOR_DEF.args.count-1)
                    sbAppend(buff, ", ");
            }
            sbAppend(buff, ")");

            break;
        case AST_THROW_STATEMENT: {
            sbAppend(buff, "throw ");
            appendNodeToBuff(node->data.AST_THROW_STATEMENT.newStmt, buff);
            break;
        }
        case AST_IMPORT_STATEMENT:
            sbAppend(buff, "import ");

            List(astNodePtr) importChain = node->data.AST_IMPORT_STATEMENT.importChain;

            for (size_t i = 0; i < importChain.count; i++) {
                CometASTNode* ident = *get(importChain, i);
                sbAppend(buff, "%s", ident->data.AST_IDENTIFIER.ident);

                if (i < importChain.count-1) {
                    sbAppend(buff, ".");
                }
            }
            sbAppend(buff, "\n");
            break;
        case AST_BREAKPOINT_STATEMENT:
            sbAppend(buff, "breakpoint\n");
            break;

        case AST_DROP_STATEMENT:
            sbAppend(buff, "drop ");
            appendNodeToBuff(node->data.AST_DROP_STATEMENT.value, buff);
            break;

        case AST_ENUM_DEF: {
            sbAppend(buff, "enum ");
            appendNodeToBuff(node->data.AST_ENUM_DEF.ident, buff);
            sbAppend(buff, " {\n");

            List(astNodePtr) items = node->data.AST_ENUM_DEF.items;
            for (size_t i = 0; i < items.count; i++) {
                sbAppend(buff, "           ");
                appendNodeToBuff(*get(items, i), buff);

                if (i < items.count-1)
                    sbAppend(buff, ",\n");
                else
                    sbAppend(buff, "\n       }");
            }
            break;
            
        }

        case AST_AS_EXPR: {
            appendNodeToBuff(node->data.AST_AS_EXPR.left, buff);
            sbAppend(buff, " as ");
            appendNodeToBuff(node->data.AST_AS_EXPR.type, buff);
            break;
        }

        default:
            sbAppend(buff, "reached unkown node type (got %d)\n", node->nodeType);
            break;
        
    }
}

char* nodeToCStr(CometASTNode* node) {
    if (!node) return NULL;

    StringBuffer buff;
    initStringBuff(&buff);

    appendNodeToBuff(node, &buff);

    return buff.data;
}

CometASTNode* deepCopyNode(CometASTNode* node) {
    CometASTNode* out;
    switch (node->nodeType) {
        case AST_INT:
            out = AST_NODE(AST_INT, node->lineNum, node->data.AST_INT.number);
            break;
        case AST_DOUBLE:
            out = AST_NODE(AST_DOUBLE, node->lineNum, node->data.AST_DOUBLE.number);
            break;
        case AST_BOOL:
            out = AST_NODE(AST_BOOL, node->lineNum, node->data.AST_BOOL.value);
            break;
        case AST_STRING:
            out = AST_NODE(AST_STRING, node->lineNum, strdup(node->data.AST_STRING.value));
            break;
        case AST_CHAR:
            out = AST_NODE(AST_CHAR, node->lineNum, node->data.AST_CHAR.value);
            break;
        case AST_IDENTIFIER:
            out = AST_NODE(AST_IDENTIFIER, node->lineNum, strdup(node->data.AST_IDENTIFIER.ident));
            break;

        case AST_ARRAY: {
            nodeList items = newList(astNodePtr);
            for (size_t i = 0; i < node->data.AST_ARRAY.elements.count; i++) {
                append(items, deepCopyNode(*get(node->data.AST_ARRAY.elements, i)));
            }
            out = AST_NODE(AST_ARRAY, node->lineNum, items);
            break;
        }

        case AST_FUNC_CALL: {
            nodeList args = newList(astNodePtr);
            for (size_t i = 0; i < node->data.AST_FUNC_CALL.args.count; i++) {
                append(args, deepCopyNode(*get(node->data.AST_FUNC_CALL.args, i)));
            }
            out = AST_NODE(
                AST_FUNC_CALL,
                node->lineNum,
                deepCopyNode(node->data.AST_FUNC_CALL.ident),
                args
            );
            break;
        }

        case AST_ARG_DEF:
            out = AST_NODE(
                AST_ARG_DEF,
                node->lineNum,
                deepCopyNode(node->data.AST_ARG_DEF.type),
                deepCopyNode(node->data.AST_ARG_DEF.ident)
            );
            break;

        case AST_TYPE: {
            nodeList baseType = newList(astNodePtr);
            nodeList genericTypes = newList(astNodePtr);
            nodeList shape = newList(astNodePtr);
            uint32_t dims = node->data.AST_TYPE.dimensions;

            for (size_t i = 0; i < node->data.AST_TYPE.baseType.count; i++) {
                append(baseType, deepCopyNode(*get(node->data.AST_TYPE.baseType, i)));
            }
            for (size_t i = 0; i < node->data.AST_TYPE.genericTypes.count; i++) {
                append(genericTypes, deepCopyNode(*get(node->data.AST_TYPE.genericTypes, i)));
            }
            for (size_t i = 0; i < node->data.AST_TYPE.shape.count; i++) {
                append(shape, deepCopyNode(*get(node->data.AST_TYPE.shape, i)));
            }

            out = AST_NODE(
                AST_TYPE,
                node->lineNum,
                baseType,
                genericTypes,
                shape,
                dims
            );
            break;
        }

        case AST_PROGRAM: {
            CometASTNode** stmts = calloc(node->data.AST_PROGRAM.statementsArraySize, sizeof(CometASTNode*));
            for (size_t i = 0; i < node->data.AST_PROGRAM.numStatements; i++) {
                stmts[i] = deepCopyNode(node->data.AST_PROGRAM.statements[i]);
            }
            out = AST_NODE(
                AST_PROGRAM,
                node->lineNum,
                stmts,
                node->data.AST_PROGRAM.numStatements,
                node->data.AST_PROGRAM.statementsArraySize
            );
            break;
        }

        case AST_INFIX_EXPRESSION:
            out = AST_NODE(
                AST_INFIX_EXPRESSION,
                node->lineNum,
                deepCopyNode(node->data.AST_INFIX_EXPRESSION.left),
                deepCopyNode(node->data.AST_INFIX_EXPRESSION.right),
                node->data.AST_INFIX_EXPRESSION.op
            );
            break;

        case AST_PREFIX_EXPRESSION:
            out = AST_NODE(
                AST_PREFIX_EXPRESSION,
                node->lineNum,
                node->data.AST_PREFIX_EXPRESSION.op,
                deepCopyNode(node->data.AST_PREFIX_EXPRESSION.right)
            );
            break;

        case AST_EXPRESSION_STATEMENT:
            out = AST_NODE(
                AST_EXPRESSION_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_EXPRESSION_STATEMENT.expression)
            );
            break;

        case AST_ASSIGN_STATEMENT:
            out = AST_NODE(
                AST_ASSIGN_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_ASSIGN_STATEMENT.ident),
                deepCopyNode(node->data.AST_ASSIGN_STATEMENT.expression),
                deepCopyNode(node->data.AST_ASSIGN_STATEMENT.type),
                node->data.AST_ASSIGN_STATEMENT.isMutable,
                node->data.AST_ASSIGN_STATEMENT.attrib
            );
            break;

        case AST_REASSIGN_STATEMENT:
            out = AST_NODE(
                AST_REASSIGN_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_REASSIGN_STATEMENT.ident),
                deepCopyNode(node->data.AST_REASSIGN_STATEMENT.expression),
                node->data.AST_REASSIGN_STATEMENT.op
            );
            break;

        case AST_WHILE_STATEMENT:
            out = AST_NODE(
                AST_WHILE_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_WHILE_STATEMENT.expression),
                deepCopyNode(node->data.AST_WHILE_STATEMENT.program)
            );
            break;

        case AST_IF_STATEMENT:
            out = AST_NODE(
                AST_IF_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_IF_STATEMENT.expression),
                deepCopyNode(node->data.AST_IF_STATEMENT.program),
                deepCopyNode(node->data.AST_IF_STATEMENT.elseProgram)
            );
            break;

        case AST_BREAK_STATEMENT:
            out = AST_NODE(AST_BREAK_STATEMENT, node->lineNum);
            break;
        case AST_CONTINUE_STATEMENT:
            out = AST_NODE(AST_CONTINUE_STATEMENT, node->lineNum);
            break;
        case AST_BREAKPOINT_STATEMENT:
            out = AST_NODE(AST_BREAKPOINT_STATEMENT, node->lineNum);
            break;

        case AST_FUNC_DEF_STATEMENT: {
            nodeList genericTypes = newList(astNodePtr);
            nodeList args = newList(astNodePtr);

            for (size_t i = 0; i < node->data.AST_FUNC_DEF_STATEMENT.genericTypes.count; i++) {
                append(genericTypes, deepCopyNode(*get(node->data.AST_FUNC_DEF_STATEMENT.genericTypes, i)));
            }
            for (size_t i = 0; i < node->data.AST_FUNC_DEF_STATEMENT.args.count; i++) {
                append(args, deepCopyNode(*get(node->data.AST_FUNC_DEF_STATEMENT.args, i)));
            }

            out = AST_NODE(
                AST_FUNC_DEF_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_FUNC_DEF_STATEMENT.ident),
                genericTypes,
                deepCopyNode(node->data.AST_FUNC_DEF_STATEMENT.program),
                args,
                deepCopyNode(node->data.AST_FUNC_DEF_STATEMENT.returnType),
                node->data.AST_FUNC_DEF_STATEMENT.isInline,
                deepCopyNode(node->data.AST_FUNC_DEF_STATEMENT.inlineExpr),
                node->data.AST_FUNC_DEF_STATEMENT.attrib
            );
            break;
        }

        case AST_RETURN_STATEMENT:
            out = AST_NODE(
                AST_RETURN_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_RETURN_STATEMENT.expression)
            );
            break;

        case AST_STRUCT_DEF_STATEMENT: {
            nodeList genericTypes = newList(astNodePtr);
            nodeList fieldDefs = newList(astNodePtr);

            for (size_t i = 0; i < node->data.AST_STRUCT_DEF_STATEMENT.genericTypes.count; i++) {
                append(genericTypes, deepCopyNode(*get(node->data.AST_STRUCT_DEF_STATEMENT.genericTypes, i)));
            }
            for (size_t i = 0; i < node->data.AST_STRUCT_DEF_STATEMENT.fieldDefs.count; i++) {
                append(fieldDefs, deepCopyNode(*get(node->data.AST_STRUCT_DEF_STATEMENT.fieldDefs, i)));
            }

            out = AST_NODE(
                AST_STRUCT_DEF_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_STRUCT_DEF_STATEMENT.ident),
                genericTypes,
                fieldDefs,
                deepCopyNode(node->data.AST_STRUCT_DEF_STATEMENT.constructor),
                deepCopyNode(node->data.AST_STRUCT_DEF_STATEMENT.destructor),
                deepCopyNode(node->data.AST_STRUCT_DEF_STATEMENT.parentName)
            );
            break;
        }

        case AST_CONSTRUCTOR_DEF: {
            nodeList args = newList(astNodePtr);
            for (size_t i = 0; i < node->data.AST_CONSTRUCTOR_DEF.args.count; i++) {
                append(args, deepCopyNode(*get(node->data.AST_CONSTRUCTOR_DEF.args, i)));
            }
            out = AST_NODE(
                AST_CONSTRUCTOR_DEF,
                node->lineNum,
                deepCopyNode(node->data.AST_CONSTRUCTOR_DEF.program),
                args
            );
            break;
        }

        case AST_DESTRUCTOR_DEF:
            out = AST_NODE(
                AST_DESTRUCTOR_DEF,
                node->lineNum,
                deepCopyNode(node->data.AST_DESTRUCTOR_DEF.program)
            );
            break;

        case AST_AS_FUNC_DEF:
            out = AST_NODE(
                AST_AS_FUNC_DEF,
                node->lineNum,
                deepCopyNode(node->data.AST_AS_FUNC_DEF.type),
                deepCopyNode(node->data.AST_AS_FUNC_DEF.body)
            );
            break;

        case AST_NEW_STATEMENT: {
            nodeList args = newList(astNodePtr);
            for (size_t i = 0; i < node->data.AST_NEW_STATEMENT.args.count; i++) {
                append(args, deepCopyNode(*get(node->data.AST_NEW_STATEMENT.args, i)));
            }
            out = AST_NODE(
                AST_NEW_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_NEW_STATEMENT.structName),
                args
            );
            break;
        }

        case AST_OVERRIDE_STATEMENT:
            out = AST_NODE(
                AST_OVERRIDE_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_OVERRIDE_STATEMENT.funcDef)
            );
            break;

        case AST_IMPORT_STATEMENT: {
            nodeList importChain = newList(astNodePtr);
            for (size_t i = 0; i < node->data.AST_IMPORT_STATEMENT.importChain.count; i++) {
                append(importChain, deepCopyNode(*get(node->data.AST_IMPORT_STATEMENT.importChain, i)));
            }
            out = AST_NODE(AST_IMPORT_STATEMENT, node->lineNum, importChain);
            break;
        }

        case AST_TRY_STATEMENT:
            out = AST_NODE(
                AST_TRY_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_TRY_STATEMENT.tryBlock),
                deepCopyNode(node->data.AST_TRY_STATEMENT.exceptBlock),
                deepCopyNode(node->data.AST_TRY_STATEMENT.exceptionType),
                deepCopyNode(node->data.AST_TRY_STATEMENT.exceptionVarName)
            );
            break;

        case AST_THROW_STATEMENT:
            out = AST_NODE(
                AST_THROW_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_THROW_STATEMENT.newStmt)
            );
            break;

        case AST_DROP_STATEMENT:
            out = AST_NODE(
                AST_DROP_STATEMENT,
                node->lineNum,
                deepCopyNode(node->data.AST_DROP_STATEMENT.value)
            );
            break;

        case AST_ENUM_DEF: {
            nodeList items = newList(astNodePtr);
            for (size_t i = 0; i < node->data.AST_ENUM_DEF.items.count; i++) {
                append(items, deepCopyNode(*get(node->data.AST_ENUM_DEF.items, i)));
            }
            out = AST_NODE(
                AST_ENUM_DEF,
                node->lineNum,
                deepCopyNode(node->data.AST_ENUM_DEF.ident),
                items
            );
            break;
        }

        case AST_AS_EXPR:
            out = AST_NODE(
                AST_AS_EXPR,
                node->lineNum,
                deepCopyNode(node->data.AST_AS_EXPR.left),
                deepCopyNode(node->data.AST_AS_EXPR.type)
            );
            break;

        default:
            return node;
    }

    out->startCol = node->startCol;
    out->endCol = node->endCol;
    return out;
}

bool nodesAreEqual(CometASTNode* a, CometASTNode* b) {
    char* aStr = nodeToCStr(a);
    char* bStr = nodeToCStr(b);
    bool equal = strcmp(aStr, bStr) == 0;

    free(aStr);
    free(bStr);

    return equal;
}

void replaceNode(CometASTNode* parentBlock, CometASTNode* child, CometASTNode* newProgram) {
    assert(parentBlock->nodeType == AST_PROGRAM);

    struct AST_PROGRAM* prog = &parentBlock->data.AST_PROGRAM;
    struct AST_PROGRAM newProg = newProgram->data.AST_PROGRAM;

    ssize_t index = -1;
    for (size_t i = 0; i < prog->numStatements; i++) {
        if (nodesAreEqual(prog->statements[i], child)) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return;
    }

    size_t oldNumStatements = prog->numStatements;
    prog->numStatements += newProg.numStatements - 1; // -1 because child is replaced

    if (prog->statementsArraySize < prog->numStatements) {
        prog->statementsArraySize = prog->numStatements;

        CometASTNode** tmpPtr = realloc(prog->statements, sizeof(CometASTNode*) * prog->statementsArraySize);
        if (!tmpPtr) {
            printf("failed to allocate memory for new statements array (replaceNode)");
            exit(1);
        }
        prog->statements = tmpPtr;
    }

    size_t elementsToShift = oldNumStatements - (index + 1);

    if (elementsToShift > 0)
        memmove(&prog->statements[index + newProg.numStatements], &prog->statements[index + 1], sizeof(CometASTNode*) * elementsToShift);

    memcpy(&prog->statements[index], newProg.statements, sizeof(CometASTNode*) * newProg.numStatements);

}