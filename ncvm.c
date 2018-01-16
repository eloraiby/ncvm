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

#include "ncvm.h"

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
    OP_YIELD,           // yield the current thread, next execution will continue at IP + 1

    OP_MAX,
} OPCODE;

typedef struct {
   const char*  name;   // word name
   uint32_t     inVs;   // input value count
   uint32_t     outVs;  // output value count
   uint32_t     metaId; // meta data block id (types, source mapping,...)
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

    [OP_YIELD]      = { "yield",    0,  0 },

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
pushValue(Process* proc, Value v) {
    assert(proc->vsCount < proc->vsCap);
    proc->vs[proc->vsCount] = v;
    ++proc->vsCount;
}

INLINE
Value
popValue(Process* proc) {
    assert(proc->vsCount != 0);
    --proc->vsCount;
    return proc->vs[proc->vsCount];
}

INLINE
void
pushLocal(Process* proc, Value v) {
    assert(proc->lsCount < proc->lsCap);
    proc->ls[proc->lsCount] = v;
    ++proc->lsCount;
}

INLINE
Value
getLocalValue(Process* proc, uint32_t lidx) {
    assert((lidx + proc->lp) < proc->lsCount);
    return proc->ls[proc->lp + lidx];
}

INLINE
void
pushReturn(Process* proc) {
    Return r = { .fp = proc->fp, .ip = proc->ip, .lp = proc->lp };
    proc->rs[proc->rsCount] = r;
    ++proc->rsCount;
}

INLINE
void
popReturn(Process* proc) {
    --proc->rsCount;
    Return  r   = proc->rs[proc->rsCount];
    proc->fp  = r.fp;
    proc->ip  = r.ip;
    proc->lp  = r.lp;
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
vmPushString(Process* proc, const char* str) {
    uint32_t    strIdx  = proc->ss.charCount;
    while(*str) {
        assert(proc->ss.charCount < proc->ss.charCap);
        proc->ss.chars[proc->ss.charCount] = *str;
        ++str;
        ++proc->ss.charCount;
    }
    proc->ss.chars[proc->ss.charCount]    = '\0';
    ++proc->ss.charCount;

    assert(proc->ss.stringCount < proc->ss.stringCap);
    proc->ss.strings[proc->ss.stringCount]  = strIdx;

    ++proc->ss.stringCount;

    // push the string index on the value stack
    vmPushValue(proc, U32V(strIdx));
}

void
vmPopString(Process* proc) {
    assert(proc->ss.charCount > 0);
    assert(proc->ss.stringCount > 0);
    proc->ss.charCount    = proc->ss.strings[proc->ss.stringCount - 1];
    --proc->ss.stringCount;
}

uint32_t
vmTopString(Process* proc) {
    assert(proc->ss.charCount > 0);
    assert(proc->ss.stringCount > 0);
    return proc->ss.strings[proc->ss.stringCount - 1];
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
vmFetch(Process* proc) {
    VM*         vm      = proc->vm;
    uint32_t    ip      = proc->ip;
    uint32_t    fp      = proc->fp;

    assert(fp < vm->funcCount);

    const char* fName   = &vm->chars[vm->funcs[fp].nameOffset];
#ifdef LOG_LEVEL_0
    log("in %s - %d | %d :", fName, fp, ip);
#endif
    Function    func        = vm->funcs[fp];
    uint32_t    fpInsCount  = ( func.type == FT_INTERP ) ? func.u.interp.insCount : 0;
    proc->fetchState.doReturn = ( ip >= fpInsCount );

    if( !proc->fetchState.doReturn ) {
        assert( func.type == FT_INTERP );
        uint32_t*   ins         = &vm->ins[func.u.interp.insOffset];

        proc->fetchState.opcode   = ins[ip];
        ++proc->ip;
        proc->fetchState.isTail   = (proc->ip >= fpInsCount);
    }
}

void
vmSetCall(Process* proc, uint32_t word) {
    proc->fetchState.opcode     = word | OP_CALL;
    proc->fetchState.doReturn   = false;
    proc->fetchState.isTail     = false;
    proc->fp                    = word & OP_CALL_MASK;
    proc->ip                    = 0;
}

void
vmSetTailCall(Process* proc, uint32_t word) {
    proc->fetchState.opcode     = word | OP_CALL;
    proc->fetchState.doReturn   = false;
    proc->fetchState.isTail     = true;
    proc->fp                    = word & OP_CALL_MASK;
    proc->ip                    = 0;
}

void
vmExecute(Process* proc) {
    VM*     vm  = proc->vm;
    if( proc->fetchState.doReturn ) {
        popReturn(proc);    // ip exceeds instruction count, return
#ifdef LOG_LEVEL_0
        log("ret to %d:%d | rs count: %d\n", proc->fp, proc->ip, proc->rsCount);
#endif
        return;
    }

    uint32_t    opcode      = proc->fetchState.opcode;
    bool        isTail      = proc->fetchState.isTail;
    uint32_t    operation   = getOperation(opcode);
    uint32_t    operand     = getOperand(opcode);

    if( operation == OP_VALUE ) {
#ifdef LOG_LEVEL_0
        log("\t[%d] %u\n", proc->vsCount, operand);
#endif
        pushValue(proc, U32V(operand));
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
                proc->readState.s0  = popValue(proc);
#ifdef LOG_LEVEL_0
                log("\tread: %d\n", proc->readState.s0.u32);
#endif
                break;
            case 2:
                proc->readState.s1  = popValue(proc);
                proc->readState.s0  = popValue(proc);
#ifdef LOG_LEVEL_0
                log("\tread: %d, %d\n", proc->readState.s0.u32, proc->readState.s1.u32);
#endif
                break;
            case 3:
                proc->readState.s2  = popValue(proc);
                proc->readState.s1  = popValue(proc);
                proc->readState.s0  = popValue(proc);
#ifdef LOG_LEVEL_0
                log("\tread: %d, %d, %d\n", proc->readState.s0.u32, proc->readState.s1.u32, proc->readState.s2.u32);
#endif
                break;
            default:
                proc->readState.s3  = popValue(proc);
                proc->readState.s2  = popValue(proc);
                proc->readState.s1  = popValue(proc);
                proc->readState.s0  = popValue(proc);
#ifdef LOG_LEVEL_0
                log("\tread: %d, %d, %d, %d\n", proc->readState.s0.u32, proc->readState.s1.u32, proc->readState.s2.u32, proc->readState.s3.u32);
#endif
                break;
            }
        }

        switch( operand ) {

        case OP_NOP:        break;
        case OP_DUP:
            pushValue(proc, proc->readState.s0);
            pushValue(proc, proc->readState.s0);
            break;
        case OP_REV_READ_VS:pushValue(proc, proc->vs[proc->vsCount - proc->readState.s0.u32 - 1]);   break;

        case OP_U32_ADD:    pushValue(proc, U32V(proc->readState.s0.u32 + proc->readState.s1.u32));  break;
        case OP_U32_SUB:    pushValue(proc, U32V(proc->readState.s0.u32 - proc->readState.s1.u32));  break;
        case OP_U32_MUL:    pushValue(proc, U32V(proc->readState.s0.u32 * proc->readState.s1.u32));  break;
        case OP_U32_DIV:    pushValue(proc, U32V(proc->readState.s0.u32 / proc->readState.s1.u32));  break;
        case OP_U32_MOD:    pushValue(proc, U32V(proc->readState.s0.u32 % proc->readState.s1.u32));  break;

        case OP_U32_AND:    pushValue(proc, U32V(proc->readState.s0.u32 & proc->readState.s1.u32));  break;
        case OP_U32_OR:     pushValue(proc, U32V(proc->readState.s0.u32 | proc->readState.s1.u32));  break;
        case OP_U32_XOR:    pushValue(proc, U32V(proc->readState.s0.u32 ^ proc->readState.s1.u32));  break;
        case OP_U32_INV:    pushValue(proc, U32V(~proc->readState.s0.u32));                          break;

        case OP_U32_SHL:    pushValue(proc, U32V(proc->readState.s0.u32 << proc->readState.s1.u32)); break;
        case OP_U32_SHR:    pushValue(proc, U32V(proc->readState.s0.u32 >> proc->readState.s1.u32)); break;

        case OP_U32_EQ:     pushValue(proc, U32V(proc->readState.s0.u32 == proc->readState.s1.u32)); break;
        case OP_U32_NEQ:    pushValue(proc, U32V(proc->readState.s0.u32 != proc->readState.s1.u32)); break;
        case OP_U32_GEQ:    pushValue(proc, U32V(proc->readState.s0.u32 >= proc->readState.s1.u32)); break;
        case OP_U32_LEQ:    pushValue(proc, U32V(proc->readState.s0.u32 <= proc->readState.s1.u32)); break;
        case OP_U32_GT:     pushValue(proc, U32V(proc->readState.s0.u32 >  proc->readState.s1.u32)); break;
        case OP_U32_LT:     pushValue(proc, U32V(proc->readState.s0.u32 <  proc->readState.s1.u32)); break;

        case OP_COND:       // if then else (BOOL @THEN @ELSE)
            if( !isTail ) {
                pushReturn(proc);   // normal call: push return value
            }
            if( proc->readState.s0.u32 != 0 ) {
                proc->fp    = proc->readState.s1.u32;
            } else {
                proc->fp    = proc->readState.s2.u32;
            }

            proc->ip = 0;
            break;

        case OP_CALL_IND:   proc->fp    = proc->readState.s0.u32; proc->ip  = 0;        break;

        case OP_PUSH_LOCAL: pushLocal(proc, proc->readState.s0);                        break;
        case OP_READ_LOCAL: pushValue(proc, getLocalValue(proc, proc->readState.s0.u32));   break;

/*
        OP_READ_RET,

        OP_VS_SIZE,         // value stack size so far
        OP_RS_SIZE,         // return stack size so far
        OP_LS_SIZE,         // local stack size so far

        OP_CURR_FP,         // current executing function pointer (index)
        OP_CURR_IP,         // current executing instruciton pointer (index)

*/
        case OP_YIELD:  proc->exceptFlags.indiv.yF  = true; break;
        default: {
            bool    isNative    = (vm->funcs[operand].type == FT_NATIVE);
            if( isNative ) {
#ifdef LOG_LEVEL_0
                log("\t[%d] <%s>\n", proc->vsCount, fName);
#endif
                vm->funcs[operand].u.native(proc);
            } else {
                if( !isTail ) {
#ifdef LOG_LEVEL_0
                    log("\t[%d] call ", proc->vsCount);
#endif
                    pushReturn(proc);   // normal call: push return value
                } else {
#ifdef LOG_LEVEL_0
                    log("\t[%d] tail ", proc->vsCount);
#endif
                }

#ifdef LOG_LEVEL_0
                log("[%d]\t%s\n", proc->rsCount, fName);
#endif
                proc->fp    = operand;
                proc->ip    = 0;
            }
            }
        }
    }
}

void
vmNext(Process* proc) {
    vmFetch     (proc);
    vmExecute   (proc);
}


VM*
vmNew(const VMParameters* params)
{
    VM* vm  = (VM*)calloc(1, sizeof(VM));

    vm->funCap  = params->maxFunctionCount;
    vm->insCap  = params->maxInstructionCount;
    vm->charCap = params->maxCharSegmentSize;
    vm->strmCap = params->maxFileCount;


    vm->funcs       = (Function*)   calloc(params->maxFunctionCount,    sizeof(Function));
    vm->ins         = (uint32_t*)   calloc(params->maxInstructionCount, sizeof(uint32_t));
    vm->chars       = (char*)       calloc(params->maxCharSegmentSize,  1);
    vm->strms       = (Stream**)    calloc(params->maxFileCount,        sizeof(Stream*));

    Stream*     errS    = vmStreamFromFile(vm, stderr, SM_WO);
    Stream*     outS    = vmStreamFromFile(vm, stdout, SM_WO);
    Stream*     inS     = vmStreamFromFile(vm, stdin,  SM_RO);

    vmStreamPush(vm, errS);
    vmStreamPush(vm, outS);
    vmStreamPush(vm, inS);


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

    uint32_t    strmCount  = vm->strmCount;
    for( uint32_t i = 0; i < strmCount; ++i ) {
        vmStreamPop(vm);
    }
    free(vm->strms);

    free(vm->compilerState.cfs);
    free(vm->compilerState.cis);
    free(vm);
}

Process*
vmNewProcess(VM* vm,
             uint32_t maxValueCount,
             uint32_t maxLocalCount,
             uint32_t maxReturnCount,
             uint32_t maxCharCount,
             uint32_t maxStringCount)
{
    Process*    proc    = (Process*)calloc(1, sizeof(Process));
    proc->lsCap = maxLocalCount;
    proc->vsCap = maxValueCount;
    proc->rsCap = maxReturnCount;

    proc->vs        = (Value*)      calloc(maxValueCount,   sizeof(Value));
    proc->ls        = (Value*)      calloc(maxLocalCount,   sizeof(Value));
    proc->rs        = (Return*)     calloc(maxReturnCount,  sizeof(Return));

    proc->ss.chars      = (char*)calloc(maxCharCount, 1);
    proc->ss.charCap    = maxCharCount;

    proc->ss.strings    = (uint32_t*)calloc(maxStringCount, sizeof(uint32_t));
    proc->ss.stringCap  = maxStringCount;

    proc->vm    = vm;
    proc->next  = vm->procList;
    if( vm->procList != NULL ) {
        vm->procList->prev  = proc;
    }
    vm->procList    = proc;
    return proc;
}

void
vmReleaseProcess(Process* proc) {
    if( proc->prev != NULL ) {
        proc->prev->next    = proc->next;
    }

    if( proc->next != NULL ) {
        proc->next->prev    = proc->prev;
    }

    if( proc->vm->procList == proc ) {
        proc->vm->procList  = proc->next;
    }

    free(proc->vs);
    free(proc->ls);
    free(proc->rs);

    free(proc->ss.chars);
    free(proc->ss.strings);

    free(proc);
}

void
vmPushValue(Process* proc, Value v) {
    assert(proc->vsCount < proc->vsCap);
    proc->vs[proc->vsCount] = v;
    ++proc->vsCount;
}

Value
vmPopValue(Process* proc) {
    assert(proc->vsCount != 0);
    --proc->vsCount;
    return proc->vs[proc->vsCount];
}

void
vmPushReturn(Process* proc) {
    Return r = { .fp = proc->fp, .ip = proc->ip, .lp = proc->lp };
    proc->rs[proc->rsCount] = r;
    ++proc->rsCount;
}

void
vmPopReturn(Process* proc) {
    --proc->rsCount;
    Return  r   = proc->rs[proc->rsCount];
    proc->fp    = r.fp;
    proc->ip    = r.ip;
    proc->lp    = r.lp;
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

