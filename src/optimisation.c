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
        case CT_DIVIDE: return AST_NODE(AST_DOUBLE, node->lineNum, (double)expr.left->data.AST_INT.number / (double)expr.right->data.AST_INT.number);
        case CT_POW: return AST_NODE(AST_INT, node->lineNum, pow(expr.left->data.AST_INT.number, expr.right->data.AST_INT.number));
        case CT_MOD: return AST_NODE(AST_INT, node->lineNum, expr.left->data.AST_INT.number % expr.right->data.AST_INT.number);
        
        case CT_EQ_EQ: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number == expr.right->data.AST_INT.number);
        case CT_NOT_EQ: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number != expr.right->data.AST_INT.number);
        case CT_LT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number < expr.right->data.AST_INT.number);
        case CT_GT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number > expr.right->data.AST_INT.number);
        case CT_LTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number <= expr.right->data.AST_INT.number);
        case CT_GTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number >= expr.right->data.AST_INT.number);
        
        default: return node;
    }
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

CometASTNode* constantFold(CometCompiler* c, CometASTNode* ast) {
    if (!ast)
        return ast;

    switch (ast->nodeType) {
        case AST_PROGRAM: {
            for (size_t i = 0; i < ast->data.AST_PROGRAM.numStatements; i++) {
                ast->data.AST_PROGRAM.statements[i] = constantFold(c, ast->data.AST_PROGRAM.statements[i]);
            }
            break;
        }

        case AST_EXPRESSION_STATEMENT: {
            ast->data.AST_EXPRESSION_STATEMENT.expression = constantFold(c, ast->data.AST_EXPRESSION_STATEMENT.expression);
            break;
        }

        case AST_RETURN_STATEMENT: {
            ast->data.AST_RETURN_STATEMENT.expression = constantFold(c, ast->data.AST_RETURN_STATEMENT.expression);
            break;
        }

        case AST_FUNC_DEF_STATEMENT: {
            ast->data.AST_FUNC_DEF_STATEMENT.inlineExpr = constantFold(c, ast->data.AST_FUNC_DEF_STATEMENT.inlineExpr);
            ast->data.AST_FUNC_DEF_STATEMENT.program = constantFold(c, ast->data.AST_FUNC_DEF_STATEMENT.program);
            break;
        }

        case AST_IF_STATEMENT: {
            ast->data.AST_IF_STATEMENT.expression = constantFold(c, ast->data.AST_IF_STATEMENT.expression);
            ast->data.AST_IF_STATEMENT.program = constantFold(c, ast->data.AST_IF_STATEMENT.program);
            ast->data.AST_IF_STATEMENT.elseProgram = constantFold(c, ast->data.AST_IF_STATEMENT.elseProgram);
            break;
        }

        case AST_ASSIGN_STATEMENT: {
            ast->data.AST_ASSIGN_STATEMENT.expression = constantFold(c, ast->data.AST_ASSIGN_STATEMENT.expression);
            break;
        }

        case AST_REASSIGN_STATEMENT: {
            ast->data.AST_REASSIGN_STATEMENT.expression = constantFold(c, ast->data.AST_REASSIGN_STATEMENT.expression);
            break;
        }

        case AST_PREFIX_EXPRESSION: {
            ast->data.AST_PREFIX_EXPRESSION.right = constantFold(c, ast->data.AST_PREFIX_EXPRESSION.right);
            
            if (!nodeIsALiteral(ast->data.AST_PREFIX_EXPRESSION.right)) {
                break;
            }

            return foldPrefixExpr(c, ast);
        }

        case AST_STRUCT_DEF_STATEMENT: {
            nodeList fieldDefs = ast->data.AST_STRUCT_DEF_STATEMENT.fieldDefs;
            for (size_t i = 0; i < fieldDefs.count; i++) {
                CometASTNode* fieldDef = *get(fieldDefs, i);

                switch (fieldDef->nodeType) {
                    case AST_ASSIGN_STATEMENT: {
                        fieldDef->data.AST_ASSIGN_STATEMENT.expression = constantFold(c, fieldDef->data.AST_ASSIGN_STATEMENT.expression);
                        break;
                    }

                    case AST_FUNC_DEF_STATEMENT: {
                        fieldDef->data.AST_FUNC_DEF_STATEMENT.program = constantFold(c, fieldDef->data.AST_FUNC_DEF_STATEMENT.program);
                        break;
                    }

                    case AST_OVERRIDE_STATEMENT: {
                        fieldDef->data.AST_OVERRIDE_STATEMENT.funcDef->data.AST_FUNC_DEF_STATEMENT.program = constantFold(c, fieldDef->data.AST_OVERRIDE_STATEMENT.funcDef->data.AST_FUNC_DEF_STATEMENT.program);
                        break;
                    }

                    default: break;
                }
            }

            if (ast->data.AST_STRUCT_DEF_STATEMENT.constructor) {
                ast->data.AST_STRUCT_DEF_STATEMENT.constructor = constantFold(c, ast->data.AST_STRUCT_DEF_STATEMENT.constructor);
            }

            break;
        }

        case AST_INFIX_EXPRESSION: {
            ast->data.AST_INFIX_EXPRESSION.left = constantFold(c, ast->data.AST_INFIX_EXPRESSION.left);
            ast->data.AST_INFIX_EXPRESSION.right = constantFold(c, ast->data.AST_INFIX_EXPRESSION.right);

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

CometASTNode* constantPropogate(CometCompiler* c, CometASTNode* ast, ConstantEnv* currentEnv) {
    if (!ast) return ast;

    switch (ast->nodeType) {
        case AST_PROGRAM: {
            for (size_t i = 0; i < ast->data.AST_PROGRAM.numStatements; i++) {
                ast->data.AST_PROGRAM.statements[i] = constantPropogate(c, ast->data.AST_PROGRAM.statements[i], currentEnv);
            }
            break;
        }

        case AST_FUNC_DEF_STATEMENT: {

            currentEnv = newConstantEnv(currentEnv, "function");
            ast->data.AST_AS_FUNC_DEF.body = constantPropogate(c, ast->data.AST_FUNC_DEF_STATEMENT.program, currentEnv);
            currentEnv = destroyConstantEnv(currentEnv);

            break;
        }

        case AST_INFIX_EXPRESSION: {
            ast->data.AST_INFIX_EXPRESSION.left = constantPropogate(c, ast->data.AST_INFIX_EXPRESSION.left, currentEnv);
            ast->data.AST_INFIX_EXPRESSION.right = constantPropogate(c, ast->data.AST_INFIX_EXPRESSION.right, currentEnv);
            break;
        }

        case AST_RETURN_STATEMENT: {
            ast->data.AST_RETURN_STATEMENT.expression = constantPropogate(c, ast->data.AST_RETURN_STATEMENT.expression, currentEnv);
            break;
        }

        case AST_ASSIGN_STATEMENT: {
            char* varName = ast->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;

            ast->data.AST_ASSIGN_STATEMENT.expression = constantPropogate(c, ast->data.AST_ASSIGN_STATEMENT.expression, currentEnv);

            CometASTNode* expr = ast->data.AST_ASSIGN_STATEMENT.expression;
            
            if (expr && nodeIsALiteral(expr)) {
                defineConstant(currentEnv, varName, expr);
            } else { // x isnt constant
                removeConstant(currentEnv, varName);
            }
            break;
        }

        case AST_REASSIGN_STATEMENT: {
            char* varName = ast->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;
            CometASTNode* expr = ast->data.AST_ASSIGN_STATEMENT.expression;

            
            if (nodeIsALiteral(expr)) {
                defineConstant(currentEnv, varName, expr);
            } else { // x isnt constant
                removeConstant(currentEnv, varName);
            }
            break;
        }

        case AST_FOR_STATEMENT: {
            ast->data.AST_FOR_STATEMENT.start = constantPropogate(c, ast->data.AST_FOR_STATEMENT.start, currentEnv);
            ast->data.AST_FOR_STATEMENT.end = constantPropogate(c, ast->data.AST_FOR_STATEMENT.end, currentEnv);
            ast->data.AST_FOR_STATEMENT.step = constantPropogate(c, ast->data.AST_FOR_STATEMENT.step, currentEnv);
            ast->data.AST_FOR_STATEMENT.program = constantPropogate(c, ast->data.AST_FOR_STATEMENT.program, currentEnv);


            ConstantRecord* current, *tmp;
            HASH_ITER(hh, currentEnv->records, current, tmp) {
                removeConstant(currentEnv, current->name);
            }
            
            
            break;
        }

        case AST_WHILE_STATEMENT: {
            ast->data.AST_WHILE_STATEMENT.expression = constantPropogate(c, ast->data.AST_WHILE_STATEMENT.expression, currentEnv);
            ast->data.AST_WHILE_STATEMENT.program = constantPropogate(c, ast->data.AST_WHILE_STATEMENT.program, currentEnv);

            ConstantRecord* current, *tmp;
            HASH_ITER(hh, currentEnv->records, current, tmp) {
                removeConstant(currentEnv, current->name);
            }
            break;
        }

        case AST_IF_STATEMENT: {
            ast->data.AST_IF_STATEMENT.expression = constantPropogate(c, ast->data.AST_IF_STATEMENT.expression, currentEnv);

            ConstantEnv* thenEnv = newConstantEnv(currentEnv, "then");
            ast->data.AST_IF_STATEMENT.program = constantPropogate(c, ast->data.AST_IF_STATEMENT.program, thenEnv);

            ConstantEnv* elseEnv = newConstantEnv(currentEnv, "else");
            ast->data.AST_IF_STATEMENT.elseProgram = constantPropogate(c, ast->data.AST_IF_STATEMENT.elseProgram, elseEnv);

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

        case AST_STRUCT_DEF_STATEMENT: {
            nodeList fieldDefs = ast->data.AST_STRUCT_DEF_STATEMENT.fieldDefs;
            for (size_t i = 0; i < fieldDefs.count; i++) {
                CometASTNode* fieldDef = *get(fieldDefs, i);

                switch (fieldDef->nodeType) {
                    case AST_ASSIGN_STATEMENT: {
                        fieldDef->data.AST_ASSIGN_STATEMENT.expression = constantPropogate(c, fieldDef->data.AST_ASSIGN_STATEMENT.expression, currentEnv);
                        break;
                    }

                    case AST_FUNC_DEF_STATEMENT: {
                        fieldDef->data.AST_FUNC_DEF_STATEMENT.program = constantPropogate(c, fieldDef->data.AST_FUNC_DEF_STATEMENT.program, currentEnv);
                        break;
                    }

                    case AST_OVERRIDE_STATEMENT: {
                        fieldDef->data.AST_OVERRIDE_STATEMENT.funcDef->data.AST_FUNC_DEF_STATEMENT.program = constantPropogate(
                            c,
                            fieldDef->data.AST_OVERRIDE_STATEMENT.funcDef->data.AST_FUNC_DEF_STATEMENT.program,
                            currentEnv
                        );
                        break;
                    }

                    default: break;
                }
            }

            if (ast->data.AST_STRUCT_DEF_STATEMENT.constructor) {
                ast->data.AST_STRUCT_DEF_STATEMENT.constructor = constantPropogate(c, ast->data.AST_STRUCT_DEF_STATEMENT.constructor, currentEnv);
            }

            break;
        }

        case AST_IDENTIFIER: {
            char* varName = ast->data.AST_IDENTIFIER.ident;
            ConstantRecord* constantValue = findConstant(currentEnv, varName);

            if (constantValue && constantValue->value) {
                return constantValue->value;
            }

            break;
        }

        default: break;
    }

    return ast;
}

CometASTNode* controlFlowSimplify(CometCompiler* c, CometASTNode* ast) {
    if (!ast) return ast;

    switch (ast->nodeType) {
        case AST_PROGRAM: {
            for (size_t i = 0; i < ast->data.AST_PROGRAM.numStatements; i++) {
                ast->data.AST_PROGRAM.statements[i] = controlFlowSimplify(c, ast->data.AST_PROGRAM.statements[i]);
            }
            break;
        }

        case AST_FUNC_DEF_STATEMENT: {
            ast->data.AST_AS_FUNC_DEF.body = controlFlowSimplify(c, ast->data.AST_FUNC_DEF_STATEMENT.program);
            break;
        }

        case AST_IF_STATEMENT: {
            CometASTNode* expr = ast->data.AST_IF_STATEMENT.expression;

            if (expr->nodeType == AST_BOOL && expr->data.AST_BOOL.value == false) {
                return NULL;
            } 
            break;
        }

        case AST_WHILE_STATEMENT: {
            CometASTNode* expr = ast->data.AST_WHILE_STATEMENT.expression;

            if (expr->nodeType == AST_BOOL && expr->data.AST_BOOL.value == false) {
                return NULL;
            } 
            break;
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
        ast = constantFold(c, ast);
        ast = constantPropogate(c, ast, constantEnv);
        ast = controlFlowSimplify(c, ast);

        char* current = nodeToCStr(ast);

        if (showPasses) {
            printf(ESC_BOLD "==> Optimisation Pass %zu" ESC_RESET "\n%s\n", pass + 1, current);
        }

        if (strcmp(previous, current) == 0)
            break;

        pass++;
        if (pass >= MAX_OPTIMISE_PASSES)
            break;

        previous = current;
    }
    
}