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
            CometASTNode* expr = ast->data.AST_ASSIGN_STATEMENT.expression;

            
            if (nodeIsALiteral(expr)) {
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

        case AST_IF_STATEMENT: {
            ast->data.AST_IF_STATEMENT.expression = constantPropogate(c, ast->data.AST_IF_STATEMENT.expression, currentEnv);

            ConstantEnv* thenEnv = newConstantEnv(currentEnv, "then");
            ast->data.AST_IF_STATEMENT.program = constantPropogate(c, ast->data.AST_IF_STATEMENT.program, thenEnv);

            ConstantEnv* elseEnv = newConstantEnv(currentEnv, "else");
            ast->data.AST_IF_STATEMENT.elseProgram = constantPropogate(c, ast->data.AST_IF_STATEMENT.elseProgram, elseEnv);

            ConstantRecord* current, *tmp;
            HASH_ITER(hh, currentEnv->records, current, tmp) {
                char* varName = current->name;

                ConstantRecord* thenRecord = findConstantLocal(thenEnv, varName);
                if (!thenRecord) {
                    removeConstant(currentEnv, varName);
                    continue;
                }

                // if there is no else branch then any changes mutate the variable, making it no longer a constant
                if (!ast->data.AST_IF_STATEMENT.elseProgram) {
                    removeConstant(currentEnv, varName);
                    continue;
                }

                ConstantRecord* elseRecord = findConstantLocal(elseEnv, varName);
                if (elseRecord) {
                    if (nodesAreEqual(thenRecord->value, elseRecord->value)) {
                        // both branches got the same value
                        defineConstant(currentEnv, varName, thenRecord->value);
                    } else {
                        // both branches reached two different values, we can't use the variable as a constant
                        removeConstant(currentEnv, varName);
                    }
                } else {
                    // then branch mutated the variable and the else branch didn't
                    removeConstant(currentEnv, varName);
                }

            }

            if (ast->data.AST_IF_STATEMENT.elseProgram) {
                ConstantRecord* current, *tmp;
                HASH_ITER(hh, currentEnv->records, current, tmp) {
                    char* varName = current->name;

                    // if the else branch modifies a variable that the then branch doesn't then the value is mutated and no longer a constant
                    if (!findConstantLocal(thenEnv, varName)) {
                        removeConstant(currentEnv, varName);
                    }   
                }
            }

            destroyConstantEnv(thenEnv);
            destroyConstantEnv(elseEnv);
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

void runOptimisations(CometCompiler* c, CometASTNode* ast) {
    char* previous = nodeToCStr(ast);

    ConstantEnv* constantEnv = newConstantEnv(NULL, "root");

    size_t pass = 0;
    while (true) {
        
        ast = constantFold(c, ast);
        ast = constantPropogate(c, ast, constantEnv);

        char* current = nodeToCStr(ast);

        if (strcmp(previous, current) == 0)
            break;

        pass++;
        if (pass >= MAX_OPTIMISE_PASSES)
            break;

        previous = current;
    }

    

    
}