#pragma once

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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef NDEBUG
#   define log(...)
#else
#   define log(...)    fprintf(stderr, __VA_ARGS__)
#endif

#ifdef __GNUC__
#   define INLINE static inline __attribute__((unused))
#else
#   define INLINE static inline
#endif

#ifndef __cplusplus
#define false   0
#define true    (!false)
typedef uint32_t    bool;
#endif

typedef struct VM   VM;

// these are made as defines because in ISO the enum values are limited to 0x7FFFFFFF
#define OP_VALUE    0x00000000
#define OP_CALL     0x80000000

#define MAX_TOKEN_SIZE  1023

typedef struct {
    uint32_t        insOffset;
    uint32_t        insCount;
} InterpFunction;

typedef void (*NativeFunction)(VM* vm);

typedef enum {
    FT_INTERP,
    FT_NATIVE,
} FunctionType;

typedef struct {
    FunctionType    type;
    bool            isImmediate;
    uint32_t        nameOffset;
    union {
        InterpFunction  interp;
        NativeFunction  native;
    } u;
} Function;

typedef struct {
    uint32_t        fp;     // function pointer
    uint32_t        ip;     // next instruction address
} Return;

typedef struct {
    uint32_t        charCount;
    uint32_t        charCap;
    char*           chars;
    uint32_t        stringCount;
    uint32_t        stringCap;
    uint32_t*       strings;
} StringStack;

typedef struct {
    bool            bf;         // boolean flag
    bool            vsOF;       // value stack overflow flag
    bool            vsUF;       // value stack underflow flag
    bool            rsOF;       // return stack overflow flag
    bool            rsUF;       // return stack underflow flag
    bool            fnOF;       // function count overflow flag
    bool            insOF;      // instruction count overflow flag
    bool            chOF;       // character segment overflow flag
} Flags;

typedef struct {
    uint32_t        funcId;     // function index
    uint32_t        ciStart;    // compiler instruction start
} CompiledFunctionEntry;

struct VM {
    bool            quit;

    uint32_t        funcCount;
    uint32_t        funCap;
    Function*       funcs;      // function segment

    uint32_t        insCount;
    uint32_t        insCap;
    uint32_t*       ins;        // code segment

    uint32_t        charCount;
    uint32_t        charCap;
    char*           chars;      // constant char segment

    uint32_t        vsCount;
    uint32_t        vsCap;
    uint32_t*       vs;         // value stack

    uint32_t        rsCount;
    uint32_t        rsCap;
    Return*         rs;         // return stack

    uint32_t        fp;         // current executing function
    uint32_t        ip;         // pointer to the next instruction to fetch

    Flags           flags;

    uint32_t        fsCount;
    uint32_t        fsCap;
    FILE**          fs;         // file stack

    StringStack     ss;         // string stack

    // compiler section
    uint32_t        cfsCount;   // compiled function stack count
    uint32_t        cfsCap;
    CompiledFunctionEntry*  cfs;

    uint32_t        cisCount;   // compiler instruction count
    uint32_t        cisCap;
    uint32_t*       cis;
};

typedef struct {
    const char*     name;
    bool            isImmediate;
    NativeFunction  native;
} NativeFunctionEntry;


INLINE
void
vmPushValue(VM* vm, uint32_t v) {
    assert(vm->vsCount < vm->vsCap);
    vm->vs[vm->vsCount] = v;
    ++vm->vsCount;
}

INLINE
uint32_t
vmPopValue(VM* vm) {
    assert(vm->vsCount != 0);
    --vm->vsCount;
    return vm->vs[vm->vsCount];
}

INLINE
void
vmPushReturn(VM* vm) {
    Return r = { .fp = vm->fp, .ip = vm->ip };
    vm->rs[vm->rsCount] = r;
    ++vm->rsCount;
}

INLINE
void
vmPopReturn(VM* vm) {
    --vm->rsCount;
    Return  r   = vm->rs[vm->rsCount];
    vm->fp  = r.fp;
    vm->ip  = r.ip;
}

INLINE
uint32_t
vmGetOperation(uint32_t opcode) {
    return opcode & 0x80000000;
}

INLINE
uint32_t
vmGetOperand(uint32_t opcode) {
    return opcode & 0x7FFFFFFF;
}

INLINE
void
vmPushInstruction(VM* vm, uint32_t opcode) {
    assert(vm->insCount < vm->insCap);
    vm->ins[vm->insCount++] = opcode;
}

INLINE
void
vmPopInstruction(VM* vm) {
    assert(vm->insCount > 0);
    --vm->insCount;
}

INLINE
void
vmPushCompilerInstruction(VM* vm, uint32_t opcode) {
    assert(vm->cisCount < vm->cisCap);
    vm->cis[vm->cisCount++] = opcode;
}

INLINE
void
vmPopCompilerInstruction(VM* vm) {
    assert(vm->cisCount > 0);
    --vm->cisCount;
}

void    vmPushString(VM* vm, const char* str);
void    vmPopString(VM* vm);

//
// start looking for the function from the last added entry
// Return: 0        -> not found
//         v != 0   -> function index + 1 (decrement to get the function)
//
uint32_t    vmFindFunction(VM* vm, const char* str);
uint32_t    vmAllocateInterpFunction(VM* vm, const char* str);
uint32_t    vmAddNativeFunction(VM* vm, const char* str, bool isImmediate, NativeFunction native);

void        vmRegisterStdWords(VM* vm);

void        vmNext(VM* vm);

void        vmReadEvalPrintLoop(VM* vm);


