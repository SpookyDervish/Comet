#include "compiler.h"
#include "ast.h"
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
    };

    if (!new.usedRegisters || !new.usedFloatRegisters) {
        return Error(CometCompiler, charptr, "Failed to allocate memory for register array!");
    }

    return Success(CometCompiler, charptr, new);
}

ResultType(Nothing, charptr) visitProgram(CometCompiler* compiler, CometASTNode* node) {
    Tram_ParameterList parameters = Tram_ParameterList_Create(1, (Tram_Parameter[]){Tram_Parameter_Variable("main")});
    Tram_Program_AddInstruction(compiler->program, Tram_Instruction_Create(Tram_InstructionType_CreateLabel, parameters));
    
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

    ResultType(ValStructPair, charptr) value = resolveValue(compiler, node->data.AST_ASSIGN_STATEMENT.expression);

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

ResultType(Nothing, charptr) visitExpressionStatement(CometCompiler* compiler, CometASTNode* node) {
    return compile(compiler, node->data.AST_EXPRESSION_STATEMENT.expression);
}

// -- HELPER FUNCTIONS -- //
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
        Tram_Register reg = allocRegister(compiler).as.success;

        Tram_ParameterList parameters = Tram_ParameterList_Create(
        2,
        (Tram_Parameter[]){
            Tram_Parameter_Register(reg),
            leftVal.as.success.val
            }
        );

        Tram_Program_AddInstruction(compiler->program, Tram_Instruction_Create(Tram_InstructionType_Put, parameters));

        parameters = Tram_ParameterList_Create(
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

    ValStructPair result = (ValStructPair){
        .val = value,
        .type = type
    };

    return Success(ValStructPair, charptr, result);
}

ResultType(Nothing, charptr) visitFuncDefStatement(CometCompiler* compiler, CometASTNode* node) {

}

// -- MAIN --//
ResultType(Nothing, charptr) compile(CometCompiler* compiler, CometASTNode* node) {

    switch (node->nodeType) {
        case AST_PROGRAM:
            return visitProgram(compiler, node);
        case AST_ASSIGN_STATEMENT:
            return visitAssignStatement(compiler, node);
        case AST_EXPRESSION_STATEMENT:
            return visitExpressionStatement(compiler, node);
        case AST_FUNC_DEF_STATEMENT:
            return visitFuncDefStatement(compiler, node);

        case AST_INFIX_EXPRESSION:
            ResultType(ValStructPair, charptr) infixExpr = visitInfixExpression(compiler, node);
            if (infixExpr.error)
                return Error(Nothing, charptr, infixExpr.as.error);

            break;

        default:
            char buffer[128];
            sprintf(buffer, "No visit method for %s node.", ASTNodeTypeToCStr(node->nodeType));
            return Error(Nothing, charptr, buffer);
    }

    return Success(Nothing, charptr, {});
}