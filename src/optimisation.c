#include "optimisation.h"

CometASTNode* foldBoolExpr(CometCompiler* c, CometASTNode* node) {
    struct AST_INFIX_EXPRESSION expr = node->data.AST_INFIX_EXPRESSION;

    ResultType(CometType, ErrorMessage) leftType = resolveType(c, expr.left);
    ResultType(CometType, ErrorMessage) rightType = resolveType(c, expr.left);

    if (leftType.error || rightType.error)
        return node;

    if ((typeIsInt(leftType.as.success) && typeIsFloat(rightType.as.success)) || (typeIsFloat(leftType.as.success) && typeIsInt(rightType.as.success)))
        return node;

    if (typeIsInt(leftType.as.success) && typeIsInt(rightType.as.success)) {
        switch (expr.op.type) {
            case CT_EQ_EQ: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number == expr.right->data.AST_INT.number);
            case CT_LT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number < expr.right->data.AST_INT.number);
            case CT_GT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number > expr.right->data.AST_INT.number);
            case CT_LTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number <= expr.right->data.AST_INT.number);
            case CT_GTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_INT.number >= expr.right->data.AST_INT.number);
            default: return node;    
        }
    } else {
        switch (expr.op.type) {
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
        case CT_LT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number < expr.right->data.AST_DOUBLE.number);
        case CT_GT: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number > expr.right->data.AST_DOUBLE.number);
        case CT_LTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number <= expr.right->data.AST_DOUBLE.number);
        case CT_GTE: return AST_NODE(AST_BOOL, node->lineNum, expr.left->data.AST_DOUBLE.number >= expr.right->data.AST_DOUBLE.number);

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
            ast = foldIntExpr(ast->data.AST_EXPRESSION_STATEMENT.expression);
            break;
        }

        case AST_RETURN_STATEMENT: {
            ast->data.AST_RETURN_STATEMENT.expression = constantFold(c, ast->data.AST_RETURN_STATEMENT.expression);
            break;
        }

        case AST_FUNC_DEF_STATEMENT: {
            struct AST_FUNC_DEF_STATEMENT funcDef = ast->data.AST_FUNC_DEF_STATEMENT;
            ast->data.AST_FUNC_DEF_STATEMENT.inlineExpr = constantFold(c, funcDef.inlineExpr);
            ast->data.AST_FUNC_DEF_STATEMENT.program = constantFold(c, funcDef.program);
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

void runOptimisations(CometCompiler* c, CometASTNode* ast) {
    printNode(ast);
    printf("\n");
    ast = constantFold(c, ast);
    printNode(ast);
    printf("\n");

}