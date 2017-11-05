/*
** Copyright (c) 2017 Wael El Oraiby.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU Affero General Public License as
**  published by the Free Software Foundation, either version 3 of the
**  License, or (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU Affero General Public License for more details.
**
**  You should have received a copy of the GNU Affero General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "nanoforth.h"

typedef enum {
    OP_NOP      = 0,

    OP_DROP,
    OP_DUP,
    OP_READ_VS,

    OP_U32_ADD,
    OP_U32_SUB,
    OP_U32_MUL,
    OP_U32_DIV,
    OP_U32_MOD,

    OP_U32_AND,
    OP_U32_OR,
    OP_U32_XOR,
    OP_U32_INV,

    OP_U32_SHL,
    OP_U32_SHR,

    OP_U32_EQ,
    OP_U32_NEQ,
    OP_U32_GEQ,
    OP_U32_LEQ,
    OP_U32_GT,
    OP_U32_LT,

    OP_COND,            // if then else (BOOL @THEN @ELSE)

    OP_PUSH_LOCAL,
    OP_READ_LOCAL,
/*
    OP_READ_RET,

    OP_VS_SIZE,         // value stack size so far
    OP_RS_SIZE,         // return stack size so far
    OP_LS_SIZE,         // local stack size so far

    OP_CURR_FP,         // current executing function pointer (index)
    OP_CURR_IP,         // current executing instruciton pointer (index)
*/
    OP_MAX,
} OPCODE;

typedef struct {
   const char*  name;
   uint32_t     inVs;
   uint32_t     outVs;
} Opcode;

static Opcode opcodes[OP_MAX] = {
    [OP_NOP    ]    = { "nop",      0,  0 },
    [OP_DROP   ]    = { "drop",     1,  0 },
    [OP_DUP    ]    = { "dup",      1,  1 },
    [OP_READ_VS]    = { "@>",       1,  1 },

    [OP_U32_ADD]    = { "+",        2,  1 },
    [OP_U32_SUB]    = { "-",        2,  1 },
    [OP_U32_MUL]    = { "*",        2,  1 },
    [OP_U32_DIV]    = { "/",        2,  1 },
    [OP_U32_MOD]    = { "%",        2,  1 },

    [OP_U32_AND]    = { "&",        2,  1 },
    [OP_U32_OR ]    = { "|",        2,  1 },
    [OP_U32_XOR]    = { "^",        2,  1 },
    [OP_U32_INV]    = { "~",        2,  1 },

    [OP_U32_SHR]    = { ">>",       2,  1 },
    [OP_U32_SHL]    = { "<<",       2,  1 },

    [OP_U32_EQ ]    = { "=",        2,  1 },
    [OP_U32_NEQ]    = { "!=",       2,  1 },
    [OP_U32_GEQ]    = { ">=",       2,  1 },
    [OP_U32_LEQ]    = { "<=",       2,  1 },
    [OP_U32_GT ]    = { ">",        2,  1 },
    [OP_U32_LT ]    = { "<",        2,  1 },

    [OP_COND   ]    = { "?",        3,  0 },

    [OP_PUSH_LOCAL] = { ">l",       2,  0 },
    [OP_READ_LOCAL] = { "l>",       1,  1 },
/*
    OP_READ_RET,

    OP_VS_SIZE,         // value stack size so far
    OP_RS_SIZE,         // return stack size so far
    OP_LS_SIZE,         // local stack size so far

    OP_CURR_FP,         // current executing function pointer (index)
    OP_CURR_IP,         // current executing instruciton pointer (index)
*/

};

static
uint32_t
addConstString(VM* vm, const char* str) {
    uint32_t    strIdx  = vm->charCount;
    while(*str) {
        vm->chars[vm->charCount] = *str;
        ++str;
        ++vm->charCount;
    }
    vm->chars[vm->charCount]    = '\0';
    ++vm->charCount;
    return strIdx;
}


void
vmPushString(VM* vm, const char* str) {
    uint32_t    strIdx  = vm->ss.charCount;
    while(*str) {
        assert(vm->ss.charCount < vm->ss.charCap);
        vm->ss.chars[vm->ss.charCount] = *str;
        ++str;
        ++vm->ss.charCount;
    }
    vm->ss.chars[vm->ss.charCount]    = '\0';
    ++vm->ss.charCount;

    assert(vm->ss.stringCount < vm->ss.stringCap);
    vm->ss.strings[vm->ss.stringCount]  = strIdx;
    ++vm->ss.stringCount;
}

void
vmPopString(VM* vm) {
    assert(vm->ss.charCount > 0);
    assert(vm->ss.stringCount > 0);
    vm->ss.charCount    = vm->ss.strings[vm->ss.stringCount - 1];
    --vm->ss.stringCount;
}

//
// start looking for the function from the last added entry
// Return: 0        -> not found
//         v != 0   -> function index + 1 (decrement to get the function)
//
uint32_t
vmFindFunction(VM* vm, const char* str) {
    for(uint32_t fidx = vm->funcCount; fidx > 0; --fidx) {
        if(strcmp(str, &vm->chars[vm->funcs[fidx - 1].nameOffset]) == 0) {
            return fidx;
        }
    }
    return 0;
}

uint32_t
vmAllocateInterpFunction(VM* vm, const char* str) {
    Function    f   = {
        .type           = FT_INTERP,
        .isImmediate    = false,
        .nameOffset     = addConstString(vm, str),
        .u              = { .interp = { .insOffset = 0, .insCount = 0 } }
    };
    uint32_t    fidx    = vm->funcCount;
    vm->funcs[vm->funcCount]    = f;
    ++vm->funcCount;
    return fidx;
}

uint32_t
vmAddNativeFunction(VM* vm, const char* str, bool isImmediate, NativeFunction native, uint32_t inVS, uint32_t outVS) {
    Function    f   = {
        .type           = FT_NATIVE,
        .isImmediate    = isImmediate,
        .nameOffset     = addConstString(vm, str),
        .u              = { .native = native },
        .inVS           = inVS,
        .outVS          = outVS
    };

    uint32_t    fidx    = vm->funcCount;
    vm->funcs[vm->funcCount]    = f;
    ++vm->funcCount;
    return fidx;
}

void
vmSetFetch(VM* vm, uint32_t opcode) {
    vm->fetchState.doReturn = true;
    vm->fetchState.isTail   = false;
    vm->fetchState.opcode   = opcode;
}

void
vmFetch(VM* vm) {
    uint32_t    ip      = vm->ip;
    uint32_t    fp      = vm->fp;

    assert(fp < vm->funcCount);

    const char* fName   = &vm->chars[vm->funcs[fp].nameOffset];
    log("in %s - %d | %d :", fName, fp, ip);

    Function    func        = vm->funcs[fp];
    uint32_t    fpInsCount  = ( func.type == FT_INTERP ) ? func.u.interp.insCount : 0;
    vm->fetchState.doReturn = ( ip >= fpInsCount );

    if( !vm->fetchState.doReturn ) {
        assert( func.type == FT_INTERP );
        uint32_t*   ins         = &vm->ins[func.u.interp.insOffset];

        vm->fetchState.opcode   = ins[ip];
        ++vm->ip;
        vm->fetchState.isTail   = (vm->ip >= fpInsCount);
    }
}

void
vmSetOpcode(VM* vm, uint32_t opcode) {
    vm->fetchState.opcode   = opcode;
    vm->fetchState.doReturn = false;
    vm->fetchState.isTail   = false;
    vm->fp                  = opcode & 0x7FFFFFFF;
    vm->ip                  = 0;
}

void
vmExecute(VM* vm) {
    uint32_t    opcode      = vm->fetchState.opcode;
    bool        isTail      = vm->fetchState.isTail;
    uint32_t    operation   = vmGetOperation(opcode);
    uint32_t    operand     = vmGetOperand(opcode);

    if( vm->fetchState.doReturn ) {
        log("\tret - ");
        vmPopReturn(vm);    // ip exceeds instruction count, return
        log("%d:%d | %d\n", vm->fp, vm->ip, vm->rsCount);
        return;
    }

    if( operation == OP_VALUE ) {
        log("\t[%d] %u\n", vm->vsCount, operand);
        vmPushValue(vm, operand);
    } else { // OP_CALL
        const char* fName       = &vm->chars[vm->funcs[operand].nameOffset];
        uint32_t    argCount    = vm->funcs[operand].inVS;

        if(operand < OP_MAX) {
            switch( argCount ) {
            case 0:
                break;
            case 1:
                vm->readState.s0    = vmPopValue(vm);
                log("\tread: %d\n", vm->readState.s0);
                break;
            case 2:
                vm->readState.s1    = vmPopValue(vm);
                vm->readState.s0    = vmPopValue(vm);
                log("\tread: %d, %d\n", vm->readState.s0, vm->readState.s1);
                break;
            case 3:
                vm->readState.s2    = vmPopValue(vm);
                vm->readState.s1    = vmPopValue(vm);
                vm->readState.s0    = vmPopValue(vm);
                log("\tread: %d, %d, %d\n", vm->readState.s0, vm->readState.s1, vm->readState.s2);
                break;
            default:
                vm->readState.s3    = vmPopValue(vm);
                vm->readState.s2    = vmPopValue(vm);
                vm->readState.s1    = vmPopValue(vm);
                vm->readState.s0    = vmPopValue(vm);
                log("\tread: %d, %d, %d, %d\n", vm->readState.s0, vm->readState.s1, vm->readState.s2, vm->readState.s3);
                break;
            }
        }

        switch( operand ) {

        case OP_NOP:
            break;

        case OP_DUP:
            vmPushValue(vm, vm->readState.s0);
            break;

        case OP_READ_VS:
            vmPushValue(vm, vm->vs[vm->vsCount - vm->readState.s0 - 1]);
            break;

        case OP_U32_ADD:
            vmPushValue(vm, vm->readState.s0 + vm->readState.s1);
            break;

        case OP_U32_SUB:
            vmPushValue(vm, vm->readState.s0 - vm->readState.s1);
            break;

        case OP_U32_MUL:
            vmPushValue(vm, vm->readState.s0 * vm->readState.s1);
            break;

        case OP_U32_DIV:
            vmPushValue(vm, vm->readState.s0 / vm->readState.s1);
            break;

        case OP_U32_MOD:
            vmPushValue(vm, vm->readState.s0 % vm->readState.s1);
            break;

        case OP_U32_AND:
            vmPushValue(vm, vm->readState.s0 & vm->readState.s1);
            break;

        case OP_U32_OR:
            vmPushValue(vm, vm->readState.s0 | vm->readState.s1);
            break;

        case OP_U32_XOR:
            vmPushValue(vm, vm->readState.s0 ^ vm->readState.s1);
            break;

        case OP_U32_INV:
            vmPushValue(vm, ~vm->readState.s0);
            break;

        case OP_U32_SHL:
            vmPushValue(vm, vm->readState.s0 << vm->readState.s1);
            break;

        case OP_U32_SHR:
            vmPushValue(vm, vm->readState.s0 >> vm->readState.s1);
            break;

        case OP_U32_EQ:
            vmPushValue(vm, vm->readState.s0 == vm->readState.s1);
            break;

        case OP_U32_NEQ:
            vmPushValue(vm, vm->readState.s0 != vm->readState.s1);
            break;

        case OP_U32_GEQ:
            vmPushValue(vm, vm->readState.s0 >= vm->readState.s1);
            break;

        case OP_U32_LEQ:
            vmPushValue(vm, vm->readState.s0 <= vm->readState.s1);
            break;

        case OP_U32_GT:
            vmPushValue(vm, vm->readState.s0 > vm->readState.s1);
            break;

        case OP_U32_LT:
            vmPushValue(vm, vm->readState.s0 < vm->readState.s1);
            break;

        case OP_COND:       // if then else (BOOL @THEN @ELSE)
            if( vm->readState.s0 != 0 ) {
                vm->fp  = vm->readState.s1;
            } else {
                vm->fp  = vm->readState.s2;
            }

            vm->ip = 0;
            break;

        case OP_PUSH_LOCAL:
            vmPushLocal(vm, vm->readState.s0);
            break;

        case OP_READ_LOCAL:
            vmGetLocalValue(vm, vm->readState.s1);
            break;
/*
        OP_READ_RET,

        OP_VS_SIZE,         // value stack size so far
        OP_RS_SIZE,         // return stack size so far
        OP_LS_SIZE,         // local stack size so far

        OP_CURR_FP,         // current executing function pointer (index)
        OP_CURR_IP,         // current executing instruciton pointer (index)

*/
        default: {
            bool    isNative    = (vm->funcs[operand].type == FT_NATIVE);
            if( isNative ) {
                log("\t[%d] <%s>\n", vm->vsCount, fName);
                vm->funcs[operand].u.native(vm);
            } else {
                if( !isTail ) {
                    log("\t[%d] call ", vm->vsCount);
                    vmPushReturn(vm);   // normal call: push return value
                } else {
                    log("\t[%d] tail ", vm->vsCount);
                }

                log("[%d]\t%s ", vm->rsCount, fName);

                vm->fp  = operand;
                vm->ip  = 0;
            }
            }
        }
    }
}

void
vmNext(VM* vm) {
    vmFetch(vm);
    vmExecute(vm);
}


VM*
vmNew(const VMParameters* params)
{
    VM* vm  = (VM*)calloc(1, sizeof(VM));

    vm->funCap  = params->maxFunctionCount;
    vm->insCap  = params->maxInstructionCount;
    vm->charCap = params->maxCharSegmentSize;
    vm->vsCap   = params->maxValuesCount;
    vm->rsCap   = params->maxReturnCount;
    vm->strmCap = params->maxFileCount;

    vm->funcs   = (Function*)   calloc(params->maxFunctionCount,    sizeof(Function));
    vm->ins     = (uint32_t*)   calloc(params->maxInstructionCount, sizeof(uint32_t));
    vm->chars   = (char*)       calloc(params->maxCharSegmentSize,  1);
    vm->vs      = (uint32_t*)   calloc(params->maxValuesCount,      sizeof(uint32_t));
    vm->rs      = (Return*)     calloc(params->maxReturnCount,      sizeof(Return));
    vm->strms   = (Stream**)    calloc(params->maxFileCount,        sizeof(Stream*));

    Stream*     errS    = vmStreamFromFile(vm, stderr, SM_WO);
    Stream*     outS    = vmStreamFromFile(vm, stdout, SM_WO);
    Stream*     inS     = vmStreamFromFile(vm, stdin,  SM_RO);

    vmStreamPush(vm, errS);
    vmStreamPush(vm, outS);
    vmStreamPush(vm, inS);

    vm->ss.chars        = (char*)calloc(params->maxSSCharCount, 1);
    vm->ss.charCap      = params->maxSSCharCount;

    vm->ss.strings      = (uint32_t*)calloc(params->maxSSStringCount, sizeof(uint32_t));
    vm->ss.stringCap    = params->maxSSStringCount;

    vm->compilerState.cfs       = (CompiledFunctionEntry*)calloc(params->maxCFCount, sizeof(CompiledFunctionEntry));
    vm->compilerState.cfsCap    = params->maxCFCount;

    vm->compilerState.cis       = (uint32_t*)calloc(params->maxCISCount, sizeof(uint32_t));
    vm->compilerState.cisCap    = params->maxCISCount;

    for(uint32_t i = 0; i < sizeof(opcodes) / sizeof(Opcode); ++i) {
        vmAddNativeFunction(vm, opcodes[i].name, false, 0, opcodes[i].inVs, opcodes[i].outVs);
    }

    vmRegisterStdWords(vm);
    return vm;
}

void
vmRelease(VM* vm) {
    free(vm->funcs);
    free(vm->ins);
    free(vm->chars);
    free(vm->vs);
    free(vm->rs);

    for( uint32_t i = 0; i < vm->strmCount; ++i ) {
        vmStreamPop(vm, vm->strms[i]);
    }
    free(vm->strms);

    free(vm->ss.chars);
    free(vm->ss.strings);
    free(vm->compilerState.cfs);
    free(vm->compilerState.cis);
    free(vm);
}
