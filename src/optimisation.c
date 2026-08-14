#include "optimisation.h"

CometASTNode* foldBoolExpr(CometCompiler* c, CometASTNode* node) {
    struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

    ResultType(CometType, ErrorMessage) leftType = resolveType(c, expr.left);
    ResultType(CometType, ErrorMessage) rightType = resolveType(c, expr.right);

    if (leftType.error || rightType.error)
        return node;

    if ((typeIsInt(leftType.as.success) && typeIsFloat(rightType.as.success)) || (typeIsFloat(leftType.as.success) && typeIsInt(rightType.as.success)))
        return node;

    if (typeIsInt(leftType.as.success) && typeIsInt(rightType.as.success)) {
        switch (expr.op.type) {
            case CT_EQ_EQ: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number == expr.right->data.AST_INT.number);
            case CT_NOT_EQ: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number != expr.right->data.AST_INT.number);
            case CT_LT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number < expr.right->data.AST_INT.number);
            case CT_GT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number > expr.right->data.AST_INT.number);
            case CT_LTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number <= expr.right->data.AST_INT.number);
            case CT_GTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number >= expr.right->data.AST_INT.number);
            default: return node;    
        }
    } else {
        switch (expr.op.type) {
            // we avoid == for decimals cause of precision errors
            case CT_LT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number < expr.right->data.AST_DOUBLE.number);
            case CT_GT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number > expr.right->data.AST_DOUBLE.number);
            case CT_LTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number <= expr.right->data.AST_DOUBLE.number);
            case CT_GTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number >= expr.right->data.AST_DOUBLE.number);
            default: return node;    
        }
    }
}

CometASTNode* foldIntExpr(CometASTNode* node) {
    struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

    switch (expr.op.type) {
        case CT_PLUS: return AST_NODE(AST_INT, node->lineNum, expr.left->data.AST_INT.number + expr.right->data.AST_INT.number);
        case CT_MINUS: return AST_NODE(AST_INT, node->lineNum, expr.left->data.AST_INT.number - expr.right->data.AST_INT.number);
        case CT_TIMES: return AST_NODE(AST_INT, node->lineNum, expr.left->data.AST_INT.number * expr.right->data.AST_INT.number);
        case CT_DIVIDE:
            if (expr.right->data.AST_INT.number == 0)
                break;

            return AST_NODE(AST_DOUBLE, node->lineNum, (double)expr.left->data.AST_INT.number / (double)expr.right->data.AST_INT.number);
        case CT_POW: return AST_NODE(AST_INT, node->lineNum, pow(expr.left->data.AST_INT.number, expr.right->data.AST_INT.number));
        case CT_MOD: return AST_NODE(AST_INT, node->lineNum, expr.left->data.AST_INT.number % expr.right->data.AST_INT.number);

        default: break;
    }

    return node;
}

CometASTNode* foldFloatExpr(CometASTNode* node) {
    struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

    switch (expr.op.type) {
        case CT_PLUS: return AST_NODE(AST_DOUBLE, node->lineNum, expr.left->data.AST_DOUBLE.number + expr.right->data.AST_DOUBLE.number);
        case CT_MINUS: return AST_NODE(AST_DOUBLE, node->lineNum, expr.left->data.AST_DOUBLE.number - expr.right->data.AST_DOUBLE.number);
        case CT_TIMES: return AST_NODE(AST_DOUBLE, node->lineNum, expr.left->data.AST_DOUBLE.number * expr.right->data.AST_DOUBLE.number);
        case CT_DIVIDE: return AST_NODE(AST_DOUBLE, node->lineNum, (double)expr.left->data.AST_DOUBLE.number / (double)expr.right->data.AST_DOUBLE.number);
        case CT_POW: return AST_NODE(AST_DOUBLE, node->lineNum, pow(expr.left->data.AST_DOUBLE.number, expr.right->data.AST_DOUBLE.number));

        case CT_EQ_EQ: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number == expr.right->data.AST_DOUBLE.number);
        case CT_NOT_EQ: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number != expr.right->data.AST_DOUBLE.number);
        case CT_LT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number < expr.right->data.AST_DOUBLE.number);
        case CT_GT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number > expr.right->data.AST_DOUBLE.number);
        case CT_LTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number <= expr.right->data.AST_DOUBLE.number);
        case CT_GTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number >= expr.right->data.AST_DOUBLE.number);

        default: return node;
    }
}

CometASTNode* constantPropogate(CometCompiler* c, ConstantEnv* currentEnv, CometASTNode* ast, CometASTNode* parentBlock);
CometASTNode* optimizerWalkAST(
    CometCompiler* c,
    ConstantEnv* currentEnv,
    CometASTNode* ast,
    CometASTNode* parentBlock,
    CometASTNode* (callback)(CometCompiler* c, ConstantEnv* env, CometASTNode* node, CometASTNode* parentBlock)) {
    
    if (!ast) return NULL;

    CometASTNode* out = callback(c, currentEnv, ast, parentBlock);

    switch (ast->nodeType) {
        case AST_PROGRAM: {
            for (size_t i = 0; i < ast->data.AST_PROGRAM.numStatements; i++) {
                ast->data.AST_PROGRAM.statements[i] = optimizerWalkAST(c, currentEnv, ast->data.AST_PROGRAM.statements[i], ast, callback);
            }
            break;
        }

        case AST_WHILE_STATEMENT: {
            ast->data.AST_WHILE_STATEMENT.expression = optimizerWalkAST(c, currentEnv, ast->data.AST_WHILE_STATEMENT.expression, parentBlock, callback);
            ast->data.AST_WHILE_STATEMENT.program = optimizerWalkAST(c, currentEnv, ast->data.AST_WHILE_STATEMENT.program, parentBlock, callback);
            break;
        }

        case AST_FOR_STATEMENT: {
            ast->data.AST_FOR_STATEMENT.start = optimizerWalkAST(c, currentEnv, ast->data.AST_FOR_STATEMENT.start, parentBlock, callback);
            ast->data.AST_FOR_STATEMENT.end = optimizerWalkAST(c, currentEnv, ast->data.AST_FOR_STATEMENT.end, parentBlock, callback);
            ast->data.AST_FOR_STATEMENT.program = optimizerWalkAST(c, currentEnv, ast->data.AST_FOR_STATEMENT.program, parentBlock, callback);
            break;
        }

        case AST_INFIX_EXPRESSION: {
            ast->data.AST_INFIX_EXPRESSION.left = optimizerWalkAST(c, currentEnv, ast->data.AST_INFIX_EXPRESSION.left, parentBlock, callback);
            ast->data.AST_INFIX_EXPRESSION.right = optimizerWalkAST(c, currentEnv, ast->data.AST_INFIX_EXPRESSION.right, parentBlock, callback);
            break;
        }

        case AST_FUNC_DEF_STATEMENT: {
            ast->data.AST_FUNC_DEF_STATEMENT.program = optimizerWalkAST(c, currentEnv, ast->data.AST_FUNC_DEF_STATEMENT.program, parentBlock, callback);
            ast->data.AST_FUNC_DEF_STATEMENT.inlineExpr = optimizerWalkAST(c, currentEnv, ast->data.AST_FUNC_DEF_STATEMENT.inlineExpr, parentBlock, callback);
            break;
        }

        case AST_FUNC_CALL: {
            for (size_t i = 0; i < ast->data.AST_FUNC_CALL.args.count; i++) {
                CometASTNode* arg = *get(ast->data.AST_FUNC_CALL.args, i);
                ast->data.AST_FUNC_CALL.args.pointer[i] = optimizerWalkAST(c, currentEnv, arg, parentBlock, callback);
            }
            
            break;
        }

        case AST_EXPRESSION_STATEMENT: {
            ast->data.AST_EXPRESSION_STATEMENT.expression = optimizerWalkAST(c, currentEnv, ast->data.AST_EXPRESSION_STATEMENT.expression, parentBlock, callback);
            break;
        }

        case AST_RETURN_STATEMENT: {
            ast->data.AST_RETURN_STATEMENT.expression = optimizerWalkAST(c, currentEnv, ast->data.AST_RETURN_STATEMENT.expression, parentBlock, callback);
            break;
        }

        case AST_IF_STATEMENT: {
            ast->data.AST_IF_STATEMENT.expression = optimizerWalkAST(c, currentEnv, ast->data.AST_IF_STATEMENT.expression, parentBlock, callback);

            if (callback != constantPropogate) {
                ast->data.AST_IF_STATEMENT.program = optimizerWalkAST(c, currentEnv, ast->data.AST_IF_STATEMENT.program, parentBlock, callback);
                ast->data.AST_IF_STATEMENT.elseProgram = optimizerWalkAST(c, currentEnv, ast->data.AST_IF_STATEMENT.elseProgram, parentBlock, callback);
            }
            break;
        }

        case AST_ASSIGN_STATEMENT: {
            ast->data.AST_ASSIGN_STATEMENT.expression = optimizerWalkAST(c, currentEnv, ast->data.AST_ASSIGN_STATEMENT.expression, parentBlock, callback);
            break;
        }

        case AST_REASSIGN_STATEMENT: {
            ast->data.AST_REASSIGN_STATEMENT.expression = optimizerWalkAST(c, currentEnv, ast->data.AST_REASSIGN_STATEMENT.expression, parentBlock, callback);
            break;
        }

        case AST_PREFIX_EXPRESSION: {
            ast->data.AST_PREFIX_EXPRESSION.right = optimizerWalkAST(c, currentEnv, ast->data.AST_PREFIX_EXPRESSION.right, parentBlock, callback);
            break;
        }

        case AST_STRUCT_DEF_STATEMENT: {
            nodeList fieldDefs = ast->data.AST_STRUCT_DEF_STATEMENT.fieldDefs;
            for (size_t i = 0; i < fieldDefs.count; i++) {
                CometASTNode* fieldDef = *get(fieldDefs, i);

                switch (fieldDef->nodeType) {
                    case AST_ASSIGN_STATEMENT: {
                        fieldDef->data.AST_ASSIGN_STATEMENT.expression = optimizerWalkAST(c, currentEnv, fieldDef->data.AST_ASSIGN_STATEMENT.expression, parentBlock, callback);
                        break;
                    }

                    case AST_FUNC_DEF_STATEMENT: {
                        fieldDef->data.AST_FUNC_DEF_STATEMENT.program = optimizerWalkAST(c, currentEnv, fieldDef->data.AST_FUNC_DEF_STATEMENT.program, parentBlock, callback);
                        break;
                    }

                    case AST_OVERRIDE_STATEMENT: {
                        fieldDef->data.AST_OVERRIDE_STATEMENT.funcDef->data.AST_FUNC_DEF_STATEMENT.program = optimizerWalkAST(
                            c,
                            currentEnv,
                            fieldDef->data.AST_OVERRIDE_STATEMENT.funcDef->data.AST_FUNC_DEF_STATEMENT.program,
                            parentBlock,
                            callback
                        );
                        break;
                    }

                    case AST_AS_FUNC_DEF: {
                        fieldDef->data.AST_AS_FUNC_DEF.body = optimizerWalkAST(
                            c,
                            currentEnv,
                            fieldDef->data.AST_AS_FUNC_DEF.body,
                            parentBlock,
                            callback
                        );
                        break;
                    }

                    default: break;
                }
            }

            if (ast->data.AST_STRUCT_DEF_STATEMENT.constructor) {
                ast->data.AST_STRUCT_DEF_STATEMENT.constructor = optimizerWalkAST(c,
                    currentEnv,
                    ast->data.AST_STRUCT_DEF_STATEMENT.constructor,
                    parentBlock,
                    callback
                );
            }
            if (ast->data.AST_STRUCT_DEF_STATEMENT.destructor) {
                ast->data.AST_STRUCT_DEF_STATEMENT.destructor = optimizerWalkAST(c,
                    currentEnv,
                    ast->data.AST_STRUCT_DEF_STATEMENT.destructor,
                    parentBlock,
                    callback
                );
            }

            break;
        }

        default: break;
    }

    return out;
}

CometASTNode* foldPrefixExpr(CometCompiler* c, CometASTNode* node) {
    struct AST_PREFIX_EXPRESSION expr = node->data.AST_PREFIX_EXPRESSION;

    ResultType(CometType, ErrorMessage) rightType = resolveType(c, expr.right);
    if (rightType.error)
        return node;

    
    switch (expr.op.type) {
        case CT_NOT: {
            switch (rightType.as.success.typeKind) {
                case COMET_BOOL:
                case COMET_SMALL:
                case COMET_INT:
                case COMET_BIG:
                    return AST_NODE(AST_BOOL, node->lineNum, !expr.right->data.AST_INT.number);

                case COMET_FLOAT:
                case COMET_DOUBLE:
                    return AST_NODE(AST_BOOL, node->lineNum, !expr.right->data.AST_DOUBLE.number);

                default: return node;
            }
        }

        case CT_MINUS: {
            switch (rightType.as.success.typeKind) {
                case COMET_BOOL:
                case COMET_SMALL:
                case COMET_INT:
                case COMET_BIG:
                    return AST_NODE(AST_INT, node->lineNum, -expr.right->data.AST_INT.number);

                case COMET_FLOAT:
                case COMET_DOUBLE:
                    return AST_NODE(AST_DOUBLE, node->lineNum, -expr.right->data.AST_DOUBLE.number);

                default: return node;
            }
        }

        default: return node;
    }
}

CometASTNode* constantFold(CometCompiler* c, ConstantEnv* env, CometASTNode* ast, CometASTNode* parentBlock) {
    (void)env; // do this to tell gcc we're using the arg;
    (void)parentBlock;

    if (!ast) return NULL;

    switch (ast->nodeType) {
        case AST_PREFIX_EXPRESSION: {
            if (!nodeIsALiteral(ast->data.AST_PREFIX_EXPRESSION.right)) {
                break;
            }

            return foldPrefixExpr(c, ast);
        }

        case AST_INFIX_EXPRESSION: {
            if (!nodeIsALiteral(ast->data.AST_INFIX_EXPRESSION.left) ||
                !nodeIsALiteral(ast->data.AST_INFIX_EXPRESSION.right)) {
                break;
            }

            ResultType(CometType, ErrorMessage) type = resolveType(c, ast);
            if (type.error) {
                break;
            }

            switch (type.as.success.typeKind) {
                case COMET_BOOL: {
                    return foldBoolExpr(c, ast);
                }

                case COMET_SMALL:
                case COMET_INT:
                case COMET_BIG: {
                    return foldIntExpr(ast);
                }

                case COMET_FLOAT:
                case COMET_DOUBLE: {
                    return foldFloatExpr(ast);
                }

                default:
                    break;
            }

            break;
        }

        default: break;
    }

    return ast;
}

CometASTNode* constantPropogate(CometCompiler* c, ConstantEnv* currentEnv, CometASTNode* ast, CometASTNode* parentBlock) {
    if (!ast) return NULL;

    switch (ast->nodeType) {
        case AST_ASSIGN_STATEMENT: {
            char* varName = ast->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;
            CometASTNode* expr = ast->data.AST_ASSIGN_STATEMENT.expression;
            
            if (expr && nodeIsALiteral(expr)) {
                defineConstant(currentEnv, varName, deepCopyNode(expr));
            } else { // x isnt constant
                removeConstant(currentEnv, varName);
            }
            break;
        }

        case AST_REASSIGN_STATEMENT: {
            char* varName = ast->data.AST_REASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;
            CometASTNode* expr = ast->data.AST_REASSIGN_STATEMENT.expression;
            CometToken op = ast->data.AST_REASSIGN_STATEMENT.op;

            if (nodeIsALiteral(expr) && op.type == CT_EQ) {
                defineConstant(currentEnv, varName, deepCopyNode(expr));
            } else { // x isnt constant
                removeConstant(currentEnv, varName);
            }
            break;
        }

        case AST_FOR_STATEMENT:
        case AST_WHILE_STATEMENT:
        case AST_FUNC_DEF_STATEMENT: {
            ConstantRecord* current, *tmp;
            HASH_ITER(hh, currentEnv->records, current, tmp) {
                removeConstant(currentEnv, current->name);
            }
            
            break;
        }

        case AST_IF_STATEMENT: {
            ConstantEnv* thenEnv = newConstantEnv(currentEnv, "then");
            ast->data.AST_IF_STATEMENT.program = optimizerWalkAST(c, thenEnv, ast->data.AST_IF_STATEMENT.program, parentBlock, constantPropogate);

            ConstantEnv* elseEnv = newConstantEnv(currentEnv, "else");
            ast->data.AST_IF_STATEMENT.elseProgram = optimizerWalkAST(c, elseEnv, ast->data.AST_IF_STATEMENT.elseProgram, parentBlock, constantPropogate);

            // create a union of all three environments
            ConstantRecord* current, *tmp;

            ConstantEnv* allEnvs = newConstantEnv(currentEnv, "all");
            HASH_ITER(hh, currentEnv->records, current, tmp) {
                defineConstant(allEnvs, current->name, current->value);
            }
            HASH_ITER(hh, thenEnv->records, current, tmp) {
                defineConstant(allEnvs, current->name, current->value);
            }
            HASH_ITER(hh, elseEnv->records, current, tmp) {
                defineConstant(allEnvs, current->name, current->value);
            }



            HASH_ITER(hh, allEnvs->records, current, tmp) {
                char* varName = current->name;
                
                ConstantRecord* before = findConstant(currentEnv, varName);

                if (!before)
                    continue;
                

                ConstantRecord* thenRecord = findConstantLocal(thenEnv, varName);
                ConstantRecord* elseRecord = findConstantLocal(elseEnv, varName);

                if (!ast->data.AST_IF_STATEMENT.elseProgram) {
                    
                    if (thenRecord && !nodesAreEqual(thenRecord->value, before->value)) {
                        removeConstant(currentEnv, varName);
                    }

                    continue;
                }

                // has else
                CometASTNode* thenVal = thenRecord != NULL ? thenRecord->value : before->value;
                CometASTNode* elseVal = elseRecord != NULL ? elseRecord->value : before->value;

                if (thenVal == NULL || elseVal == NULL) {
                    removeConstant(currentEnv, varName);
                } else if (nodesAreEqual(thenVal, elseVal)) {
                    defineConstant(currentEnv, current->name, thenVal);
                } else {
                    removeConstant(currentEnv, varName);
                }

            }

            destroyConstantEnv(allEnvs);
            destroyConstantEnv(thenEnv);
            destroyConstantEnv(elseEnv);
            break;
        }

        case AST_IDENTIFIER: {
            char* varName = ast->data.AST_IDENTIFIER.ident;
            ConstantRecord* constantValue = findConstant(currentEnv, varName);

            if (constantValue && constantValue->value) {
                return deepCopyNode(constantValue->value);
            }
            break;
        }

        default: break;
    }

    return ast;
}

CometASTNode* controlFlowSimplify(CometCompiler* c, ConstantEnv* env, CometASTNode* ast, CometASTNode* parentBlock) {
    (void)c; // do this to tell gcc we're using the arg;
    (void)env;

    switch (ast->nodeType) {
        case AST_IF_STATEMENT: {
            CometASTNode* expr = ast->data.AST_IF_STATEMENT.expression;
            if (expr->nodeType != AST_BOOL) break;

            if (expr->data.AST_BOOL.value == true) { // if statement is true, replace if statement with its main logic

                replaceNode(parentBlock, ast, ast->data.AST_IF_STATEMENT.program);
                return ast->data.AST_IF_STATEMENT.program->data.AST_PROGRAM.statements[0];
            } else if (ast->data.AST_IF_STATEMENT.elseProgram) { // if statement is false, if it has an else branch replace it with that

                replaceNode(parentBlock, ast, ast->data.AST_IF_STATEMENT.elseProgram);
                return ast->data.AST_IF_STATEMENT.elseProgram->data.AST_PROGRAM.statements[0];
            } else { // if statement is false and it has no else branch, just delete the whole thing
                return NULL;
            }
            break;
        }

        case AST_WHILE_STATEMENT: {
            CometASTNode* expr = ast->data.AST_IF_STATEMENT.expression;
            if (expr->nodeType != AST_BOOL) break;

            // delete the while loop if its condition will never be true
            if (expr->data.AST_BOOL.value == false) {
                return NULL;
            }
        }

        default: break;
    }

    return ast;
}

void runOptimisations(CometCompiler* c, CometASTNode* ast, bool showPasses) {
    char* previous = nodeToCStr(ast);

    ConstantEnv* constantEnv = newConstantEnv(NULL, "root");

    size_t pass = 0;
    while (true) {
        ast = optimizerWalkAST(c, constantEnv, ast, NULL, constantFold);
        ast = optimizerWalkAST(c, constantEnv, ast, NULL, constantPropogate);
        ast = optimizerWalkAST(c, constantEnv, ast, NULL, controlFlowSimplify);

        char* current = nodeToCStr(ast);

        if (showPasses) {
            printf(ESC_BOLD "==> Optimisation Pass %zu" ESC_RESET "\n%s\n", pass + 1, current);
        }

        if (strcmp(previous, current) == 0)
            break;

        pass++;
        if (pass >= MAX_OPTIMISE_PASSES)
            break;

        free(previous);
        previous = current;
    }
    
}