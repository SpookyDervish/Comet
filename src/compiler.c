#include "compiler.h"
#include "ast.h"
#include "environment.h"
#include "lexer.h"
#include "parser.h"
#include "token.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tram.h>

ResultType(Tram_Register, charptr) allocRegister(CometCompiler* compiler) {
    size_t numberOfRegs = Tram_Register_f0;

    for (size_t i = 0; i < numberOfRegs; i++) {
        if (!compiler->usedRegisters[i]) {
            compiler->usedRegisters[i] = true;
            return Success(Tram_Register, charptr, i);
        }
    }

    return Error(Tram_Register, charptr, "No more free registers.");
}

ResultType(Nothing, charptr) freeRegister(CometCompiler* compiler, Tram_Register reg) {
    compiler->usedRegisters[reg] = false;
    return Success(Nothing, charptr, {});
}

ResultType(Tram_Register, charptr) allocFloatRegister(CometCompiler* compiler) {
    size_t numberOfRegs = (Tram_Register_f15+1) - Tram_Register_f0;

    for (size_t i = 0; i < numberOfRegs; i++) {
        if (!compiler->usedFloatRegisters[i]) {
            compiler->usedFloatRegisters[i] = true;
            return Success(Tram_Register, charptr, i + Tram_Register_f0);
        }
    }

    return Error(Tram_Register, charptr, "No more free registers.");
}

ResultType(Nothing, charptr) freeFloatRegister(CometCompiler* compiler, Tram_Register reg) {
    compiler->usedFloatRegisters[reg - Tram_Register_f0] = false;
    return Success(Nothing, charptr, {});
}

ResultType(CometCompiler, charptr) createCompiler(CometParser* parser) {
    Tram_Program* program = malloc(sizeof(Tram_Program));
    if (!program) {
        return Error(CometCompiler, charptr, "Failed to allocate memory for Tram program!");
    }
    *program = Tram_Program_Create();

    CometCompiler new = {
        .program = program,
        .usedRegisters = calloc(Tram_Register_f0, sizeof(Tram_Register)),
        .usedFloatRegisters = calloc((Tram_Register_f15+1) - Tram_Register_f0, sizeof(Tram_Register)),
        .liveness = (Liveness){}
    };

    // create an environment with no parent
    new.env = newEnvironment("root", NULL);

    if (!new.usedRegisters || !new.usedFloatRegisters) {
        return Error(CometCompiler, charptr, "Failed to allocate memory for register array!");
    }

    return Success(CometCompiler, charptr, new);
}

ResultType(Nothing, charptr) visitProgram(CometCompiler* compiler, CometASTNode* node) {
    for (size_t i = 0; i < node->data.AST_PROGRAM.numStatements; i++) {
        ResultType(Nothing, charptr) result = compile(compiler, node->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;
    }

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) compileAST(CometCompiler* compiler, CometASTNode* root, char* outputName) {
    ResultType(Nothing, charptr) compileResult = compile(compiler, root);
    if (compileResult.error)
        return compileResult;

    Tram_Compiler* tramCompiler = Tram_Compiler_Create(*compiler->program);

    if (!tramCompiler) {
        return Error(Nothing, charptr, "Failed to allocate memory for Tram backend!");
    }

    Tram_Compiler_SetLogLevel(tramCompiler, Tram_LogLevel_All);
    Tram_Compiler_SetTarget(tramCompiler, Tram_Target_x86_64_Linux_libc);

    Tram_Compiler_Compile(tramCompiler);

    if (Tram_Compiler_HasCompiled(tramCompiler)) {
        printf("%s\n", Tram_Compiler_GetAsm(tramCompiler));
        Tram_Compiler_AssembleAndLink(tramCompiler, outputName, (Tram_LinkerArgs){.size = 0, .data = NULL});
    } else {
        return Error(Nothing, charptr, "Compilation failed.");
    }

    Tram_Compiler_Destroy(tramCompiler);

    return Success(Nothing, charptr, {});
}

// -- STATEMENTS -- //
ResultType(ValStructPair, charptr) resolveValue(CometCompiler* compiler, CometASTNode* node);
ResultType(Nothing, charptr) visitAssignStatement(CometCompiler* compiler, CometASTNode* node) {
    ResultType(Tram_Register, charptr) reg = allocRegister(compiler);

    if (reg.error)
        return Error(Nothing, charptr, reg.as.error);

    ResultType(ValStructPair, charptr) value = resolveValue(compiler, node->data.AST_ASSIGN_STATEMENT.expression);
    if (value.error)
        return Error(Nothing, charptr, value.as.error);

    char* ident = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;

    // check for double definition
    if (lookup(compiler->env, ident)) {
        char* buffer = malloc(128);
        sprintf(buffer, "Redefinition of variable \"%s\"", ident);
        return Error(Nothing, charptr, buffer);
    }

    // define var
    defineVar(
        compiler->env,
        ident,
        reg.as.success,
        value.as.success.type
    );

    Tram_ParameterList parameters = Tram_ParameterList_Create(
    2,
    (Tram_Parameter[]){
        Tram_Parameter_Register(reg.as.success),
        value.as.success.val
        }
    );
    
    Tram_Program_AddInstruction(compiler->program, Tram_Instruction_Create(Tram_InstructionType_Put, parameters));

    if (value.as.success.val.type == Tram_ParameterType_Register) {
        freeRegister(compiler, value.as.success.val.value.registerValue);
    }

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) visitReturnStatement(CometCompiler* compiler, CometASTNode* node) {
    CometASTNode* expr = node->data.AST_RETURN_STATEMENT.expression;

    Tram_ParameterList parameters;
    if (expr) {
        ResultType(ValStructPair, charptr) value = resolveValue(compiler, expr);
        if (value.error)
            return Error(Nothing, charptr, value.as.error);

        parameters = Tram_ParameterList_Create(
            1,
            (Tram_Parameter[]){
                value.as.success.val,
            }
            );
    } else {
        parameters = Tram_ParameterList_Create(0, (Tram_Parameter[]){});
    }

    Tram_Program_AddInstruction(compiler->program, Tram_Instruction_Create(Tram_InstructionType_Return, parameters));
    
    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) visitExpressionStatement(CometCompiler* compiler, CometASTNode* node) {
    return compile(compiler, node->data.AST_EXPRESSION_STATEMENT.expression);
}

ResultType(Nothing, charptr) visitFuncDefStatement(CometCompiler* compiler, CometASTNode* node) {
    struct AST_FUNC_DEF_STATEMENT funcDef = node->data.AST_FUNC_DEF_STATEMENT;
    char* funcName = funcDef.ident->data.AST_IDENTIFIER.ident;

    Tram_Program_AddInstruction(compiler->program, Tram_Instruction_Create(
        Tram_InstructionType_CreateLabel,
        Tram_ParameterList_Create(
            1,
            (Tram_Parameter[]){
                Tram_Parameter_Variable(funcName)
            })
        )
    );

    CometEnvironment* funcEnv = newEnvironment(funcName, compiler->env);
    compiler->env = funcEnv;
    for (size_t i = 0; i < funcDef.program->data.AST_PROGRAM.numStatements; i++) {
        ResultType(Nothing, charptr) result = compile(compiler, funcDef.program->data.AST_PROGRAM.statements[i]);
        if (result.error)
            return result;
    }

    compiler->env = funcEnv->parent;
    free(funcEnv);

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) visitReassignStatement(CometCompiler* compiler, CometASTNode* node) {
    ResultType(ValStructPair, charptr) value = resolveValue(compiler, node->data.AST_ASSIGN_STATEMENT.expression);
    if (value.error)
        return Error(Nothing, charptr, value.as.error);

    char* varIdent = node->data.AST_ASSIGN_STATEMENT.ident->data.AST_IDENTIFIER.ident;

    Record* varRecord = lookup(compiler->env, varIdent);
    if (!varRecord) {
        char* buffer = malloc(256);
        sprintf(buffer, "Undefined variable \"%s\"", varIdent);
        return Error(Nothing, charptr, buffer);
    }

    // TODO: proper type system
    if (strcmp(varRecord->type, value.as.success.type) != 0) {
        return Error(Nothing, charptr, "Can't change type of variable at runtime.");
    }

    Tram_ParameterList parameters = Tram_ParameterList_Create(
    2,
    (Tram_Parameter[]){
        Tram_Parameter_Register(varRecord->reg),
        value.as.success.val
        }
    );
    
    Tram_Program_AddInstruction(compiler->program, Tram_Instruction_Create(Tram_InstructionType_Put, parameters));
    return Success(Nothing, charptr, {});
}

// -- HELPER FUNCTIONS -- //
bool isRegisterDead(CometCompiler* compiler, Tram_Register reg) {
    return compiler->liveness.useCount[reg] <= 0;
}

ResultType(Nothing, charptr) useRegister(CometCompiler* compiler, Tram_Register reg) {
    compiler->liveness.useCount[reg]--;

    if (compiler->liveness.useCount[reg] <= 0) {
        if (reg < Tram_Register_f0)
            return freeRegister(compiler, reg);
        else
            return freeFloatRegister(compiler, reg);
    }

    return Success(Nothing, charptr, {});
}

ResultType(Nothing, charptr) countUses(CometCompiler* compiler, CometASTNode* node) {
    if (!node) return Success(Nothing, charptr, {});

    switch (node->nodeType) {
        case AST_INFIX_EXPRESSION:
            countUses(compiler, node->data.AST_INFIX_EXPRESSION.left);
            countUses(compiler, node->data.AST_INFIX_EXPRESSION.right);
            break;

        case AST_IDENTIFIER:
            char* varIdent = node->data.AST_IDENTIFIER.ident;
            Record* varRecord = lookup(compiler->env, varIdent);
            
            if (!varRecord) {
                char* buffer = malloc(256);
                sprintf(buffer, "Undefined variable \"%s\"", varIdent);
                return Error(Nothing, charptr, buffer);
            }

            compiler->liveness.useCount[varRecord->reg]++;
            break;

        default:
            break;
    }
}

ResultType(ValStructPair, charptr) visitInfixExpression(CometCompiler* compiler, CometASTNode* node);
ResultType(ValStructPair, charptr) resolveValue(CometCompiler* compiler, CometASTNode* node) {
    switch (node->nodeType) {
        case AST_INT:
            ValStructPair intRes = {
                .val = Tram_Parameter_Literal(Tram_Literal_Int(node->data.AST_INT.number)),
                .type = "int"
            };

            return Success(ValStructPair, charptr, intRes);
        case AST_DOUBLE:

            ValStructPair doubleRes = {
                .val = Tram_Parameter_Literal(Tram_Literal_Float(node->data.AST_DOUBLE.number)),
                .type = "double"
            };

            return Success(ValStructPair, charptr, doubleRes);

        case AST_IDENTIFIER:
            char* varIdent = node->data.AST_IDENTIFIER.ident;

            Record* varRecord = lookup(compiler->env, varIdent);
            if (!varRecord) {
                char* buffer = malloc(256);
                sprintf(buffer, "Undefined variable \"%s\"", varIdent);
                return Error(ValStructPair, charptr, buffer);
            }

            ValStructPair identRes = {
                .val = Tram_Parameter_Register(varRecord->reg),
                .type = "int"
            };

            return Success(ValStructPair, charptr, identRes);

        case AST_INFIX_EXPRESSION:
            return visitInfixExpression(compiler, node);

        default:
            return Error(ValStructPair, charptr, "Unsupported type for resolving.");
    }
}

// -- EXPRESSIONS -- //
ResultType(ValStructPair, charptr) visitInfixExpression(CometCompiler* compiler, CometASTNode* node) {
    CometTokenType opType = node->data.AST_INFIX_EXPRESSION.op.type;

    ResultType(ValStructPair, charptr) leftVal = resolveValue(compiler, node->data.AST_INFIX_EXPRESSION.left);
    if (leftVal.error) return leftVal;
    ResultType(ValStructPair, charptr) rightVal = resolveValue(compiler, node->data.AST_INFIX_EXPRESSION.right);
    if (rightVal.error) return rightVal;

    Tram_Parameter value;
    char* type = NULL;

    if (strcmp(leftVal.as.success.type, "int") == 0 && strcmp(rightVal.as.success.type, "int") == 0) {
        Tram_Register reg;

        if (leftVal.as.success.val.type == Tram_ParameterType_Register &&
            isRegisterDead(compiler, leftVal.as.success.val.value.registerValue)) {
            
            reg = leftVal.as.success.val.value.registerValue;

        } else {
            reg = allocRegister(compiler).as.success;
        }


        // avoid self copy
        if (!(leftVal.as.success.val.type == Tram_ParameterType_Register &&
            leftVal.as.success.val.value.registerValue == reg)) {
            Tram_ParameterList putParams = Tram_ParameterList_Create(
            2,
            (Tram_Parameter[]){
                Tram_Parameter_Register(reg),
                leftVal.as.success.val
                }
            );

            Tram_Program_AddInstruction(compiler->program, Tram_Instruction_Create(Tram_InstructionType_Put, putParams));
        }

        Tram_ParameterList parameters = Tram_ParameterList_Create(
        2,
        (Tram_Parameter[]){
            Tram_Parameter_Register(reg),
            rightVal.as.success.val
            }
        );
        
        switch (opType) {
            case CT_PLUS:
                type = "int";
                Tram_Program_AddInstruction(compiler->program, Tram_Instruction_Create(Tram_InstructionType_Add, parameters));
                value = Tram_Parameter_Register(reg);
                break;
            case CT_MINUS:
                type = "int";
                Tram_Program_AddInstruction(compiler->program, Tram_Instruction_Create(Tram_InstructionType_Subtract, parameters));
                value = Tram_Parameter_Register(reg);
                break;
            case CT_TIMES:
                type = "int";
                Tram_Program_AddInstruction(compiler->program, Tram_Instruction_Create(Tram_InstructionType_Multiply, parameters));
                value = Tram_Parameter_Register(reg);
                break;
            default:
                char* buffer = malloc(256);

                sprintf(buffer, "Invalid operator \"%s\" for int and int.", tokenTypeToCStr(opType));

                return Error(ValStructPair, charptr, buffer);
        }
    }

    if (leftVal.as.success.val.type == Tram_ParameterType_Register) {
        useRegister(compiler, leftVal.as.success.val.value.registerValue);
    }
    if (rightVal.as.success.val.type == Tram_ParameterType_Register) {
        useRegister(compiler, rightVal.as.success.val.value.registerValue);
    }

    ValStructPair result = (ValStructPair){
        .val = value,
        .type = type
    };

    return Success(ValStructPair, charptr, result);
}

ResultType(Nothing, charptr) visitFuncCall(CometCompiler* compiler, CometASTNode* node) {
    List(astNodePtr) funcArgs = node->data.AST_FUNC_CALL.args;
    
    Tram_Parameter* instArgs = calloc(funcArgs.count + 1, sizeof(Tram_Parameter));


    instArgs[0] = Tram_Parameter_Variable(node->data.AST_FUNC_CALL.ident->data.AST_IDENTIFIER.ident);
    for (size_t i = 0; i < funcArgs.count; i++) {
        ResultType(ValStructPair, charptr) value = resolveValue(compiler, *get(node->data.AST_FUNC_CALL.args, i));
        if (value.error)
            return Error(Nothing, charptr, value.as.error);

        instArgs[i+1] = value.as.success.val;
    }

    Tram_Program_AddInstruction(
        compiler->program,
        Tram_Instruction_Create(
            Tram_InstructionType_Call,
            Tram_ParameterList_Create(funcArgs.count + 1,instArgs)
        )
    );
    
    return Success(Nothing, charptr, {});
}

// -- MAIN --//
ResultType(Nothing, charptr) compile(CometCompiler* compiler, CometASTNode* node) {

    switch (node->nodeType) {
        case AST_PROGRAM:
            return visitProgram(compiler, node);
        case AST_ASSIGN_STATEMENT:
            return visitAssignStatement(compiler, node);
        case AST_REASSIGN_STATEMENT:
            return visitReassignStatement(compiler, node);
        case AST_EXPRESSION_STATEMENT:
            return visitExpressionStatement(compiler, node);
        case AST_FUNC_DEF_STATEMENT:
            return visitFuncDefStatement(compiler, node);
        case AST_RETURN_STATEMENT:
            return visitReturnStatement(compiler, node);

        case AST_INFIX_EXPRESSION:
            ResultType(ValStructPair, charptr) infixExpr = visitInfixExpression(compiler, node);
            if (infixExpr.error)
                return Error(Nothing, charptr, infixExpr.as.error);

            break;
        case AST_FUNC_CALL:
            return visitFuncCall(compiler, node);

        default:
            char* buffer = malloc(128);
            sprintf(buffer, "No visit method for %s node.", ASTNodeTypeToCStr(node->nodeType));
            return Error(Nothing, charptr, buffer);
    }

    return Success(Nothing, charptr, {});
}