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

#define U32V(V) (Value) { .u32 = V }

typedef enum {
    OP_NOP      = 0,

    OP_DROP,
    OP_DUP,
    OP_REV_READ_VS,     // read starting from the top of the stack

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

    OP_CALL_IND,

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
    [OP_DROP   ]    = { "vs.drop",  1,  0 },
    [OP_DUP    ]    = { "vs.dup",   1,  1 },
    [OP_REV_READ_VS]= { "vs.rev.read",  1,  1 },

    [OP_U32_ADD]    = { "u32.add",  2,  1 },
    [OP_U32_SUB]    = { "u32.sub",  2,  1 },
    [OP_U32_MUL]    = { "u32.mul",  2,  1 },
    [OP_U32_DIV]    = { "u32.div",  2,  1 },
    [OP_U32_MOD]    = { "u32.mod",  2,  1 },

    [OP_U32_AND]    = { "u32.and",  2,  1 },
    [OP_U32_OR ]    = { "u32.or",   2,  1 },
    [OP_U32_XOR]    = { "u32.xor",  2,  1 },
    [OP_U32_INV]    = { "u32.not",  2,  1 },

    [OP_U32_SHR]    = { "u32.shr",  2,  1 },
    [OP_U32_SHL]    = { "u32.shl",  2,  1 },

    [OP_U32_EQ ]    = { "u32.eq",   2,  1 },
    [OP_U32_NEQ]    = { "u32.neq",  2,  1 },
    [OP_U32_GEQ]    = { "u32.geq",  2,  1 },
    [OP_U32_LEQ]    = { "u32.leq",  2,  1 },
    [OP_U32_GT ]    = { "u32.gt",   2,  1 },
    [OP_U32_LT ]    = { "u32.lt",   2,  1 },

    [OP_COND   ]    = { "cond",     3,  0 },

    [OP_CALL_IND  ] = { "call",     1,  0 },

    [OP_PUSH_LOCAL] = { "ls.push",  1,  0 },
    [OP_READ_LOCAL] = { "ls.read",  1,  1 },

/*
    OP_READ_RET,

    OP_VS_SIZE,         // value stack size so far
    OP_RS_SIZE,         // return stack size so far
    OP_LS_SIZE,         // local stack size so far

    OP_CURR_FP,         // current executing function pointer (index)
    OP_CURR_IP,         // current executing instruciton pointer (index)
*/

};

INLINE
void
pushValue(VM* vm, Value v) {
    assert(vm->vsCount < vm->vsCap);
    vm->vs[vm->vsCount] = v;
    ++vm->vsCount;
}

INLINE
Value
popValue(VM* vm) {
    assert(vm->vsCount != 0);
    --vm->vsCount;
    return vm->vs[vm->vsCount];
}

INLINE
void
pushLocal(VM* vm, Value v) {
    assert(vm->lsCount < vm->lsCap);
    vm->ls[vm->lsCount] = v;
    ++vm->lsCount;
}

INLINE
Value
getLocalValue(VM* vm, uint32_t lidx) {
    assert((lidx + vm->lp) < vm->lsCount);
    return vm->ls[vm->lp + lidx];
}

INLINE
void
pushReturn(VM* vm) {
    Return r = { .fp = vm->fp, .ip = vm->ip, .lp = vm->lp };
    vm->rs[vm->rsCount] = r;
    ++vm->rsCount;
}

INLINE
void
popReturn(VM* vm) {
    --vm->rsCount;
    Return  r   = vm->rs[vm->rsCount];
    vm->fp  = r.fp;
    vm->ip  = r.ip;
    vm->lp  = r.lp;
}

INLINE
uint32_t
getOperation(uint32_t opcode) {
    return opcode & OP_CALL;
}

INLINE
uint32_t
getOperand(uint32_t opcode) {
    return opcode & OP_CALL_MASK;
}

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

    // push the string index on the value stack
    vmPushValue(vm, U32V(strIdx));
}

void
vmPopString(VM* vm) {
    assert(vm->ss.charCount > 0);
    assert(vm->ss.stringCount > 0);
    vm->ss.charCount    = vm->ss.strings[vm->ss.stringCount - 1];
    --vm->ss.stringCount;
}

uint32_t
vmTopString(VM* vm) {
    assert(vm->ss.charCount > 0);
    assert(vm->ss.stringCount > 0);
    return vm->ss.strings[vm->ss.stringCount - 1];
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
vmFetch(VM* vm) {
    uint32_t    ip      = vm->ip;
    uint32_t    fp      = vm->fp;

    assert(fp < vm->funcCount);

    const char* fName   = &vm->chars[vm->funcs[fp].nameOffset];
#ifdef LOG_LEVEL_0
    log("in %s - %d | %d :", fName, fp, ip);
#endif
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
vmSetCall(VM* vm, uint32_t word) {
    vm->fetchState.opcode   = word | OP_CALL;
    vm->fetchState.doReturn = false;
    vm->fetchState.isTail   = false;
    vm->fp                  = word & OP_CALL_MASK;
    vm->ip                  = 0;
}

void
vmSetTailCall(VM* vm, uint32_t word) {
    vm->fetchState.opcode   = word | OP_CALL;
    vm->fetchState.doReturn = false;
    vm->fetchState.isTail   = true;
    vm->fp                  = word & OP_CALL_MASK;
    vm->ip                  = 0;
}

void
vmExecute(VM* vm) {

    if( vm->fetchState.doReturn ) {
        popReturn(vm);    // ip exceeds instruction count, return
#ifdef LOG_LEVEL_0
        log("ret to %d:%d | rs count: %d\n", vm->fp, vm->ip, vm->rsCount);
#endif
        return;
    }

    uint32_t    opcode      = vm->fetchState.opcode;
    bool        isTail      = vm->fetchState.isTail;
    uint32_t    operation   = getOperation(opcode);
    uint32_t    operand     = getOperand(opcode);

    if( operation == OP_VALUE ) {
#ifdef LOG_LEVEL_0
        log("\t[%d] %u\n", vm->vsCount, operand);
#endif
        pushValue(vm, U32V(operand));
    } else { // OP_CALL
#ifdef LOG_LEVEL_0
        const char* fName       = &vm->chars[vm->funcs[operand].nameOffset];
#endif
        uint32_t    argCount    = vm->funcs[operand].inVS;

        if(operand < OP_MAX) {
#ifdef LOG_LEVEL_0
            log("\tOPCODE %s\n", fName);
#endif
            switch( argCount ) {
            case 0:
                break;
            case 1:
                vm->readState.s0    = popValue(vm);
#ifdef LOG_LEVEL_0
                log("\tread: %d\n", vm->readState.s0.u32);
#endif
                break;
            case 2:
                vm->readState.s1    = popValue(vm);
                vm->readState.s0    = popValue(vm);
#ifdef LOG_LEVEL_0
                log("\tread: %d, %d\n", vm->readState.s0.u32, vm->readState.s1.u32);
#endif
                break;
            case 3:
                vm->readState.s2    = popValue(vm);
                vm->readState.s1    = popValue(vm);
                vm->readState.s0    = popValue(vm);
#ifdef LOG_LEVEL_0
                log("\tread: %d, %d, %d\n", vm->readState.s0.u32, vm->readState.s1.u32, vm->readState.s2.u32);
#endif
                break;
            default:
                vm->readState.s3    = popValue(vm);
                vm->readState.s2    = popValue(vm);
                vm->readState.s1    = popValue(vm);
                vm->readState.s0    = popValue(vm);
#ifdef LOG_LEVEL_0
                log("\tread: %d, %d, %d, %d\n", vm->readState.s0.u32, vm->readState.s1.u32, vm->readState.s2.u32, vm->readState.s3.u32);
#endif
                break;
            }
        }

        switch( operand ) {

        case OP_NOP:        break;
        case OP_DUP:
            pushValue(vm, vm->readState.s0);
            pushValue(vm, vm->readState.s0);
            break;
        case OP_REV_READ_VS:pushValue(vm, vm->vs[vm->vsCount - vm->readState.s0.u32 - 1]);  break;

        case OP_U32_ADD:    pushValue(vm, U32V(vm->readState.s0.u32 + vm->readState.s1.u32));   break;
        case OP_U32_SUB:    pushValue(vm, U32V(vm->readState.s0.u32 - vm->readState.s1.u32));   break;
        case OP_U32_MUL:    pushValue(vm, U32V(vm->readState.s0.u32 * vm->readState.s1.u32));   break;
        case OP_U32_DIV:    pushValue(vm, U32V(vm->readState.s0.u32 / vm->readState.s1.u32));   break;
        case OP_U32_MOD:    pushValue(vm, U32V(vm->readState.s0.u32 % vm->readState.s1.u32));   break;

        case OP_U32_AND:    pushValue(vm, U32V(vm->readState.s0.u32 & vm->readState.s1.u32));   break;
        case OP_U32_OR:     pushValue(vm, U32V(vm->readState.s0.u32 | vm->readState.s1.u32));   break;
        case OP_U32_XOR:    pushValue(vm, U32V(vm->readState.s0.u32 ^ vm->readState.s1.u32));   break;
        case OP_U32_INV:    pushValue(vm, U32V(~vm->readState.s0.u32));                         break;

        case OP_U32_SHL:    pushValue(vm, U32V(vm->readState.s0.u32 << vm->readState.s1.u32));  break;
        case OP_U32_SHR:    pushValue(vm, U32V(vm->readState.s0.u32 >> vm->readState.s1.u32));  break;

        case OP_U32_EQ:     pushValue(vm, U32V(vm->readState.s0.u32 == vm->readState.s1.u32));  break;
        case OP_U32_NEQ:    pushValue(vm, U32V(vm->readState.s0.u32 != vm->readState.s1.u32));  break;
        case OP_U32_GEQ:    pushValue(vm, U32V(vm->readState.s0.u32 >= vm->readState.s1.u32));  break;
        case OP_U32_LEQ:    pushValue(vm, U32V(vm->readState.s0.u32 <= vm->readState.s1.u32));  break;
        case OP_U32_GT:     pushValue(vm, U32V(vm->readState.s0.u32 >  vm->readState.s1.u32));  break;
        case OP_U32_LT:     pushValue(vm, U32V(vm->readState.s0.u32 <  vm->readState.s1.u32));  break;

        case OP_COND:       // if then else (BOOL @THEN @ELSE)
            if( !isTail ) {
                pushReturn(vm);   // normal call: push return value
            }
            if( vm->readState.s0.u32 != 0 ) {
                vm->fp  = vm->readState.s1.u32;
            } else {
                vm->fp  = vm->readState.s2.u32;
            }

            vm->ip = 0;
            break;

        case OP_CALL_IND:   vm->fp  = vm->readState.s0.u32; vm->ip  = 0;                break;

        case OP_PUSH_LOCAL: pushLocal(vm, vm->readState.s0);                            break;
        case OP_READ_LOCAL: pushValue(vm, getLocalValue(vm, vm->readState.s0.u32));     break;

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
#ifdef LOG_LEVEL_0
                log("\t[%d] <%s>\n", vm->vsCount, fName);
#endif
                vm->funcs[operand].u.native(vm);
            } else {
                if( !isTail ) {
#ifdef LOG_LEVEL_0
                    log("\t[%d] call ", vm->vsCount);
#endif
                    pushReturn(vm);   // normal call: push return value
                } else {
#ifdef LOG_LEVEL_0
                    log("\t[%d] tail ", vm->vsCount);
#endif
                }

#ifdef LOG_LEVEL_0
                log("[%d]\t%s\n", vm->rsCount, fName);
#endif
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
    vm->lsCap   = params->maxLocalsCount;
    vm->vsCap   = params->maxValuesCount;
    vm->rsCap   = params->maxReturnCount;
    vm->strmCap = params->maxFileCount;

    vm->funcs   = (Function*)   calloc(params->maxFunctionCount,    sizeof(Function));
    vm->ins     = (uint32_t*)   calloc(params->maxInstructionCount, sizeof(uint32_t));
    vm->chars   = (char*)       calloc(params->maxCharSegmentSize,  1);
    vm->vs      = (Value*)      calloc(params->maxValuesCount,      sizeof(Value));
    vm->ls      = (Value*)      calloc(params->maxLocalsCount,      sizeof(Value));
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
        vmStreamPop(vm);
    }
    free(vm->strms);

    free(vm->ss.chars);
    free(vm->ss.strings);
    free(vm->compilerState.cfs);
    free(vm->compilerState.cis);
    free(vm);
}


void
vmPushValue(VM* vm, Value v) {
    assert(vm->vsCount < vm->vsCap);
    vm->vs[vm->vsCount] = v;
    ++vm->vsCount;
}

Value
vmPopValue(VM* vm) {
    assert(vm->vsCount != 0);
    --vm->vsCount;
    return vm->vs[vm->vsCount];
}

void
vmPushReturn(VM* vm) {
    Return r = { .fp = vm->fp, .ip = vm->ip, .lp = vm->lp };
    vm->rs[vm->rsCount] = r;
    ++vm->rsCount;
}

void
vmPopReturn(VM* vm) {
    --vm->rsCount;
    Return  r   = vm->rs[vm->rsCount];
    vm->fp  = r.fp;
    vm->ip  = r.ip;
    vm->lp  = r.lp;
}

void
vmPushInstruction(VM* vm, uint32_t opcode) {
    assert(vm->insCount < vm->insCap);
    vm->ins[vm->insCount++] = opcode;
}

void
vmPopInstruction(VM* vm) {
    assert(vm->insCount > 0);
    --vm->insCount;
}

void
vmPushCompilerInstruction(VM* vm, uint32_t opcode) {
    assert(vm->compilerState.cisCount < vm->compilerState.cisCap);
    vm->compilerState.cis[vm->compilerState.cisCount++] = opcode;
}

void
vmPopCompilerInstruction(VM* vm) {
    assert(vm->compilerState.cisCount > 0);
    --vm->compilerState.cisCount;
}

