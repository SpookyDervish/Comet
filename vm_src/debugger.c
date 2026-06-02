#include "debugger.h"
#include "args.h"
#include "../lib/estr.h"
#include "../lib/ansi.h"
#include "serialized.h"
#include "vm.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


static inline int64_t max(int64_t a, int64_t b) {
    return a > b ? a : b;
}

static inline int64_t min(int64_t a, int64_t b) {
    return a < b ? a : b;
}

void printDisassembly(CometDebugger* dbgr, Range range) {
    if (range.start > range.end) {
        uint32_t newEnd = range.start;
        range.start = range.end;
        range.end = newEnd;
    }

    // go from start of range to end
    for (size_t i = range.start; i <= range.end; i++) {

        // display breakpoints
        bool isAtBreakpoint = dbgr->breakpoints[i];
        if (isAtBreakpoint) {
            printf(ESC_DIM " [" ESC_RESET ESC_BOLD ESC_BOLD ESC_RED_FG "B" ESC_RESET ESC_DIM "] " ESC_RESET);
            isAtBreakpoint = true;
        } else {
            printf("     ");
        }
            

        
        // if...
        if (i < dbgr->vm->currentFrame->ip) {                   // below current address? dim
            printf(ESC_DIM "    ");
        } else if (i == dbgr->vm->currentFrame->ip) {           // at current address? bright green
            printf(ESC_BOLD ESC_BRIGHT_GREEN_FG " -> ");
        } else {                                                // after current address? normal colour
            
            printf("    ");

            
        }

        // some instructions have a special colour to make it easier to
        // spot loops and that kind of thing
        switch (dbgr->vm->instructions[i].opcode) {
            case INST_JMP:
            case INST_JMP_IF_FALSE:
            case INST_CALL:
            case INST_CALL_METHOD:
            case INST_RET:
                printf(ESC_BOLD ESC_CYAN_FG);
                break;

            default:
                
                break;
        }

        // make sure we dont read garbage
        if (i >= dbgr->vm->numInstructions) {
            

            printf("0x%04lx    ??? <invalid address>\n" ESC_RESET, i);
            break;
        }

        // print the instruction
        printf("%s\n" ESC_RESET, cometInstructionToCStr(dbgr->vm, dbgr->vm->instructions[i], i));
    }
}

Range getRangeNearIP(CometDebugger* dbgr) {

    return (Range){
        .start = max(((int64_t)dbgr->vm->currentFrame->ip) - 5, 0),
        .end = min(dbgr->vm->currentFrame->ip + 5, dbgr->vm->numInstructions-1)
    };
}

ResultType(voidPtr, charptr) continueHandler(CometDebugger* dbgr, int argc, char** argv) {
    dbgr->vm->instructionsLeftToExec = UINT64_MAX;
    dbgr->running = false;
    return Success(voidPtr, charptr, NULL);
}

ResultType(voidPtr, charptr) stepHandler(CometDebugger* dbgr, int argc, char** argv) {

    if (argc < 1) {
        dbgr->vm->instructionsLeftToExec = 1;
    } else {
        uint64_t value = strtoul(argv[0], NULL, 0);
        if (value == 0) {
            return Error(voidPtr, charptr, "invalid number input!");
        }

        dbgr->vm->instructionsLeftToExec = value;
    }
    
    dbgr->running = false;

    return Success(voidPtr, charptr, NULL);
}

ResultType(voidPtr, charptr) disassembleHandler(CometDebugger* dbgr, int argc, char** argv) {
    // get range to display
    Range range;
    if (argc < 1) 
        range = getRangeNearIP(dbgr);
    else 
        range = parseRange(argv[0]);
    
    if (range.start < 0) {
        return Error(voidPtr, charptr, "can't start disassembly at negative address!");
    }

    printDisassembly(dbgr, range) ;

    return Success(voidPtr, charptr, NULL);
}

int tokenize(char* line, char** argv, int maxArgs) {
    int argc = 0;

    while (*line) {
        // skip whitespace
        while (*line == ' ' || *line == '\t' || *line == '\n')
            line++;

        if (*line == '\0')
            break;

        if (argc >= maxArgs)
            break;

        argv[argc++] = line;

        // find end of token
        while (*line &&
              *line != ' ' &&
              *line != '\t' &&
              *line != '\n') {
            line++;
        }

        // terminate token
        if (*line) {
            *line = '\0';
            line++;
        }
    }

    return argc;
}

/// !==== INSTRUCTIONS ====! ///
static const char* helpAliases[] = {"h", NULL};
static const char* disassembleAliases[] = {"d", NULL};
static const char* breakAliases[] = {"b", NULL};
static const char* unbreakAliases[] = {"ubreak", "u", "ub", NULL};
static const char* stackAliases[] = {"st", NULL};
static const char* localAliases[] = {"l", NULL};
static const char* structsAliases[] = {"ls", NULL};
static const char* stepAliases[] = {"s", NULL};
static const char* continueAliases[] = {"c", "cont", NULL};
static const char* quitAliases[] = {"q", "stop", "exit", NULL};

ResultType(voidPtr, charptr) helpHandler(CometDebugger* dbgr, int argc, char** argv);
const CometDebugCommand DBGR_COMMANDS[] = {
    {"help", helpHandler, "display a list of all commnads or get help about a specific command", "h | h <command>", helpAliases},
    {"quit", NULL, "exit the debugger and stop the vm", "q", quitAliases},
    {"disassemble", disassembleHandler, "disassemble a line or range of lines", "d | d <line> | d <start>:<end>", disassembleAliases},
    {"break", NULL, "set a breakpoint", "b <address> | b <functionName>", breakAliases},
    {"unbreak", NULL, "delete a breakpoint", "ub <breakpointId>", unbreakAliases},
    {"stack", NULL, "display the current state of the stack", "st | st <index>", stackAliases},
    {"local", NULL, "print all variables or get the value of a variable", "l | l <name>", localAliases},
    {"structs", NULL, "print all structs or display info about a struct", "ls | ls <name>", structsAliases},
    {"step", stepHandler, "execute next instruction", "s | s <numInstructions>", stepAliases},
    {"continue", continueHandler, "continue execution", "c", continueAliases},
};
/// !==== END OF INSTRUCTIONS ====! ///

const CometDebugCommand* getCommandByName(char* cmdName) {
    for (size_t i = 0; i < sizeof(DBGR_COMMANDS)/sizeof(DBGR_COMMANDS[0]); i++) {
        CometDebugCommand cmd = DBGR_COMMANDS[i];

        if (strcmp(cmdName, cmd.name) == 0) {
            return &DBGR_COMMANDS[i];
        }

        // check command aliases
        size_t aliasIdx = 0;
        while (true) {
            if (strcmp(cmd.aliases[aliasIdx], cmdName) == 0) {
                return &DBGR_COMMANDS[i];
            }

            if (cmd.aliases[aliasIdx+1] == NULL) {
                break;
            }

            aliasIdx++;
        }
    }

    return NULL;
}

ResultType(voidPtr, charptr) parseCommand(CometDebugger* dbgr, char* line) {

    int maxArgs = 16;
    char* argv[maxArgs];
    int argc = tokenize(line, argv, maxArgs);

    if (argc == 0)
        return Success(voidPtr, charptr, NULL);

    const CometDebugCommand* cmd = getCommandByName(argv[0]);
    if (cmd == NULL) {
        Estr errMsg = CREATE_ESTR("dbgr: no such command: \"");
        APPEND_ESTR(errMsg, argv[0]);
        APPEND_ESTR(errMsg, "\"");
        fprintf(stderr, "%s\n", errMsg.str);
        DESTROY_ESTR(errMsg);

        return Success(voidPtr, charptr, NULL);
    }

    ResultType(voidPtr, charptr) cmdResult = cmd->handler(dbgr, argc - 1, &argv[1]);
    if (cmdResult.error) {
        fprintf(stderr, "%s: %s\n", cmd->name, cmdResult.as.error);
    }

    return Success(voidPtr, charptr, NULL);
}

void debuggerLoop(CometDebugger* dbgr) {
    while (dbgr->running) {
        char* userInput = NULL;
        size_t size = 0;
        ssize_t charsRead;

        printf("dbgr > ");
        charsRead = getline(&userInput, &size, stdin);
        if (charsRead == -1) {
            fprintf(stderr, "getline: failed to read input!");
            return;
        }

        ResultType(voidPtr, charptr) commandResult = parseCommand(dbgr, userInput);

        if (commandResult.error) 
            break;
        
        
    }
}

void startDebugger(CometVM* vm, bool startedFromStep) {
    CometDebugger* newDbgr = malloc(sizeof(CometDebugger));
    newDbgr->vm = vm;
    newDbgr->breakpoints = vm->breakpoints;
    newDbgr->running = true;

    if (startedFromStep) {
        printDisassembly(newDbgr, getRangeNearIP(newDbgr));
        debuggerLoop(newDbgr);
        return;
    }

    printf(ESC_BOLD ESC_CYAN_FG "\n!--- BREAKPOINT - COMET DEBUGGER ---!" ESC_RESET "\n");
    printf(ESC_BOLD "VM State:\n" ESC_RESET);
    printf("    - IP: 0x%016lx\n", vm->currentFrame->ip);
    printf("    - Current Inst: %s\n", cometInstructionToCStr(vm, vm->instructions[vm->currentFrame->ip], vm->currentFrame->ip));
    printf("    - SP: 0x%08x\n", vm->currentFrame->sp);
    printf("    - Stack: %s\n\n", stackAsString(*vm->currentStack, *vm->currentSp));
    debuggerLoop(newDbgr);

    free(newDbgr);
}

void printAliases(CometDebugCommand cmd) {
    size_t aliasIdx = 0;
    while (true) {
        if (cmd.aliases[aliasIdx+1] == NULL) {
            printf("%s\n", cmd.aliases[aliasIdx]);
            break;
        }

        printf("%s, ", cmd.aliases[aliasIdx]);

        aliasIdx++;
    }
}

ResultType(voidPtr, charptr) helpHandler(CometDebugger* dbgr, int argc, char** argv) {
    // look for command with given name
    if (argc > 0) {
        char* cmdName = argv[0];

        const CometDebugCommand* cmd = getCommandByName(argv[0]);
        if (cmd == NULL) {
            Estr errMsg = CREATE_ESTR("no such command: \"");
            APPEND_ESTR(errMsg, argv[0]);
            APPEND_ESTR(errMsg, "\"")
            return Error(voidPtr, charptr, errMsg.str);
        }

        printf(ESC_BOLD ESC_CYAN_FG "\\\\\\ %s ///\n\n" ESC_RESET, cmd->name);
        printf(ESC_BOLD ESC_YELLOW_FG "Description:" ESC_RESET " %s\n", cmd->help);
        printf(ESC_BOLD ESC_YELLOW_FG "Usage:" ESC_RESET " %s\n", cmd->usage);
        printf(ESC_BOLD ESC_YELLOW_FG "Aliases: " ESC_RESET);
        printAliases(*cmd);

        return Success(voidPtr, charptr, NULL);
    }

    // help command with no args
    printf(ESC_BOLD ESC_CYAN_FG "\\\\\\ commands ///\n\n" ESC_RESET);

    for (size_t i = 0; i < sizeof(DBGR_COMMANDS)/sizeof(DBGR_COMMANDS[0]); i++) {
        CometDebugCommand cmd = DBGR_COMMANDS[i];

        printf(ESC_BOLD ESC_YELLOW_FG "   %s" ESC_RESET " - %s | Aliases: ", cmd.name, cmd.help);

        printAliases(cmd);
    }

    return Success(voidPtr, charptr, NULL);
}

Range parseRange(const char* str) {
    Range r = {0};

    char* colon = strchr(str, ':');

    if (!colon) {
        // single value: "3"
        r.start = strtoul(str, NULL, 0);
        r.end = r.start;
        r.hasEnd = 0;
        return r;
    }

    // split in-place copy (safe version uses temp buffer)
    char buf[64];
    strncpy(buf, str, sizeof(buf));
    buf[63] = '\0';

    char* c = strchr(buf, ':');
    *c = '\0';

    r.start = strtoul(buf, NULL, 0);
    r.end = strtoul(c + 1, NULL, 0);
    r.hasEnd = 1;

    return r;
}

char* cometImmediateToCStr(CometImmediate immediate) {
    switch (immediate.typeKind) {
        case COMET_SMALL: {
            char* buffer = malloc(4);
            sprintf(buffer, "%hhd", immediate.smallVal);
            return buffer;
        }
        case COMET_INT: {
            char* buffer = malloc(32);
            sprintf(buffer, "%d", immediate.intVal);
            
            return buffer;
        }
        case COMET_BIG: {
            char* buffer = malloc(64);
            sprintf(buffer, "%zu", immediate.bigVal);
            return buffer;
        }

        case COMET_FLOAT: {
            char* buffer = malloc(128);
            sprintf(buffer, "%f", immediate.floatVal);
            return buffer;
        }
        case COMET_DOUBLE: {
            char* buffer = malloc(128);
            sprintf(buffer, "%f", immediate.doubleVal);
            return buffer;
        }

        case COMET_BOOL: {
            if (immediate.boolVal) {
                return "true";
            } else {
                return "false";
            }
        }

        case COMET_VOID: {
            return "void";
        }

        default: return "FIXME";
    }
}

char* cometOperandToCStr(CometVM* vm, CometOperand operand) {
    switch (operand.type) {
        case CO_IMMEDIATE:
            return cometImmediateToCStr(operand.imm);

        case CO_STACK: {
            char* buffer = malloc(32);
            sprintf(buffer, "%%%d", operand.stackIdx);
            return buffer;
        }

        case CO_SYMBOL: {
            CometSerializedFunc func = vm->functions[operand.symbolIdx];

            char* buffer = malloc(64);
            sprintf(buffer, "func %s, %d args", func.name, func.numArgs); 
            return buffer;
        }

        case CO_LABEL: {
            char* buffer = malloc(32);
            if (operand.label->resolved)
                sprintf(buffer, "0x%x", operand.label->pos);
            else
                sprintf(buffer, "(unresolved)");

            return buffer;
        }

        default: break;
    }

    return "FIXME";
}

char* cometInstOpcodeToCStr(CometInstType instType) {
    switch (instType) {
        case INST_PUSH_CONST   : return "    PUSH_CONST      ";
        case INST_STORE        : return "    STORE           ";
        case INST_LOAD         : return "    LOAD            ";
        case INST_ADDI         : return "    ADDI            ";
        case INST_ADDF         : return "    ADDF            ";
        case INST_SUBI         : return "    SUBI            ";
        case INST_SUBF         : return "    SUBF            ";
        case INST_MULI         : return "    MULI            ";
        case INST_MULF         : return "    MULF            ";
        case INST_DIVI         : return "    DIVI            ";
        case INST_DIVF         : return "    DIVF            ";
        case INST_LOAD_ARG     : return "    LOAD_ARG        ";
        case INST_RET          : return "    RET             ";
        case INST_CALL         : return "    CALL            ";
        case INST_EQI          : return "    EQI             ";
        case INST_EQF          : return "    EQF             ";
        case INST_NEQI         : return "    NEQI            ";
        case INST_NEQF         : return "    NEQF            ";
        case INST_LTI          : return "    LTI             ";
        case INST_LTF          : return "    LTF             ";
        case INST_GTI          : return "    GTI             ";
        case INST_GTF          : return "    GTF             ";
        case INST_LTEI         : return "    LTEI            ";
        case INST_LTEF         : return "    LTEF            ";
        case INST_GTEI         : return "    GTEI            ";
        case INST_GTEF         : return "    GTEF            ";
        case INST_JMP          : return "    JMP             ";
        case INST_JMP_IF_FALSE : return "    JMP_IF_FALSE    ";
        case INST_NOT          : return "    NOT             ";
        case INST_I2F          : return "    I2F             ";
        case INST_DUP          : return "    DUP             ";
        case INST_NEW          : return "    NEW             ";
        case INST_GET_FIELD    : return "    GET_FIELD       ";
        case INST_SET_FIELD    : return "    SET_FIELD       ";
        case INST_CALL_METHOD  : return "    CALL_METHOD     ";
        case INST_BREAKPOINT   : return "    BREAKPOINT      ";
        default                : return "    FIXME           ";
    }
}

CometOperand createOperand(CometOperandKind type) {
    return (CometOperand){
        .type = type
    };
}

CometOperand instArgToOperand(CometInstType opcode, uint32_t arg, uint32_t index) {
    // table of sadness and despair :trollface:
    const CometValueTypeKind argTypesTable[][3] = {
        {COMET_SMALL, COMET_VOID, COMET_VOID}, // INST_PUSH_CONST
        {COMET_SMALL, COMET_VOID, COMET_VOID}, // INST_STORE
        {COMET_SMALL, COMET_VOID, COMET_VOID}, // INST_LOAD
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_ADDI
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_ADDF
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_SUBI
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_SUBF
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_MULI
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_MULF
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_DIVI
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_DIVF
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_EQI
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_EQF
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_NEQI
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_NEQF
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_GTI
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_GTF
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_LTI
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_LTF
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_GTEI
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_GTEF
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_LTEI
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_LTEF
        {COMET_SMALL,  COMET_VOID, COMET_VOID}, // INST_LOAD_ARG
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_RET
        {COMET_FUNCTION, COMET_VOID, COMET_VOID}, // INST_CALL
        {COMET_SMALL,  COMET_VOID, COMET_VOID}, // INST_JMP
        {COMET_SMALL,  COMET_VOID, COMET_VOID}, // INST_JMP_IF_FALSE
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_NOT
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_I2F
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_DUP
        {COMET_SMALL,  COMET_VOID, COMET_VOID}, // INST_NEW
        {COMET_SMALL,  COMET_VOID, COMET_VOID}, // INST_GET_FIELD
        {COMET_SMALL,  COMET_VOID, COMET_VOID}, // INST_SET_FIELD
        {COMET_SMALL,  COMET_VOID, COMET_VOID}, // INST_CALL_METHOD
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_BREAKPOINT
        {COMET_VOID,  COMET_VOID, COMET_VOID}, // INST_MAX
    };

    CometValueTypeKind argTypes[3];
    memcpy(argTypes, argTypesTable[opcode], sizeof(argTypes));

    CometValueTypeKind argType = argTypes[index];

    switch (argType) {
        case COMET_SMALL:
            return (CometOperand){
                .type = CO_IMMEDIATE,
                .imm = (CometImmediate){
                    .typeKind = argType,
                    .smallVal = arg
                }
            };
        case COMET_INT:
            return (CometOperand){
                .type = CO_IMMEDIATE,
                .imm = (CometImmediate){
                    .typeKind = argType,
                    .intVal = arg
                }
            };
        case COMET_BIG:
            return (CometOperand){
                .type = CO_IMMEDIATE,
                .imm = (CometImmediate){
                    .typeKind = argType,
                    .bigVal = arg
                }
            };
        case COMET_FLOAT:
            return (CometOperand){
                .type = CO_IMMEDIATE,
                .imm = (CometImmediate){
                    .typeKind = argType,
                    .floatVal = arg
                }
            };
        case COMET_DOUBLE:
            return (CometOperand){
                .type = CO_IMMEDIATE,
                .imm = (CometImmediate){
                    .typeKind = argType,
                    .doubleVal = arg
                }
            };
        case COMET_BOOL:
            return (CometOperand){
                .type = CO_IMMEDIATE,
                .imm = (CometImmediate){
                    .typeKind = argType,
                    .boolVal = arg
                }
            };
        case COMET_FUNCTION:
            return (CometOperand){
                .type = CO_IMMEDIATE,
                .symbolIdx = arg
            };
        default:
            return (CometOperand){
                .type = CO_NONE
            };
    }
}

CometOperand* instructionArgsToOperandValues(CometSerializedInst inst) {
    CometOperand* values = calloc(3, sizeof(CometOperand));

    values[0] = instArgToOperand(inst.opcode, inst.a, 0);
    values[1] = instArgToOperand(inst.opcode, inst.a, 1);
    values[2] = instArgToOperand(inst.opcode, inst.a, 2);

    return values;

} 

char* cometInstructionToCStr(CometVM* vm, CometSerializedInst inst, uint64_t instPos) {
    char* buffer = malloc(256);
    char* extra = malloc(128);

    buffer[0] = 0;
    extra[0] = 0;

    // print extra info if we can
    switch (inst.opcode) {
        case INST_PUSH_CONST:
            
            sprintf(
                extra,
                "; consts[%d] = %s",
                inst.a,
                cometOperandToCStr(vm, vm->constants[inst.a])
            );
            break;

        case INST_CALL_METHOD: {
            CometOperand func = createOperand(CO_SYMBOL);
            func.symbolIdx = inst.a;

            sprintf(
                extra,
                "; vtable[%d] = %s",

                inst.a,
                cometOperandToCStr(vm, func)
            );
            break;
        }

        case INST_JMP:
        case INST_JMP_IF_FALSE:
            if (inst.a >= vm->numInstructions) {
                sprintf(extra, "; (???)");
                break;
            }

            sprintf(
                extra,
                "; (%s)",
                cometInstructionToCStr(vm, vm->instructions[inst.a], inst.a)
            );
            break;

        default: break;
    }

    char* argsBuffer = malloc(128);
    argsBuffer[0] = 0;
    CometOperand* args = instructionArgsToOperandValues(inst);
    
    if (args[0].type != CO_NONE)
        sprintf(argsBuffer, "%s", cometOperandToCStr(vm, args[0]));    
    if (args[1].type != CO_NONE)
        sprintf(argsBuffer + strlen(argsBuffer), ", %s", cometOperandToCStr(vm, args[1]));    
    if (args[2].type != CO_NONE)
        sprintf(argsBuffer + strlen(argsBuffer), ", %s", cometOperandToCStr(vm, args[2]));    

    sprintf(
        buffer,
        "0x%04lx%s%s    %s",
        instPos,
        cometInstOpcodeToCStr(inst.opcode),
        argsBuffer,

        extra
    );

    return buffer;
}

char* stackAsString(int64_t* stack, uint32_t sp) {
    Estr stackString = CREATE_ESTR("[");

    for (size_t i = 0; i < sp; i++) {

        char* buffer = malloc(64);

        sprintf(buffer, "0x%" PRIx64 "%s", stack[i], i < sp-1 ? ", " : "");
        APPEND_ESTR(stackString, buffer);
    }

    APPEND_ESTR(stackString, "]");

    return stackString.str;
}

char* stackTrace(CometVM* vm) {
    Estr stackTraceStr = CREATE_ESTR("\nCall Stack (most recent call first):\n");

    for (size_t i = vm->callIdx; i-- > 0;) {
        Frame* call = vm->callStack[i];

        char* funcBuffer = malloc(128);
        sprintf(funcBuffer, "    0x%04lx    %s    (sp: 0x%x)  %s\n", call->ip, call->funcName, call->sp, stackAsString(call->stack, call->sp));

        APPEND_ESTR(stackTraceStr, funcBuffer);
    }

    return stackTraceStr.str;
}