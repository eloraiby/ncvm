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
#include <stdatomic.h>
#include <assert.h>

#ifdef NDEBUG
#   define log(...)
#else
#   define log(...)    fprintf(stderr, __VA_ARGS__)
#endif

#define LOG_LEVEL_0 1

#ifdef __GNUC__
#   define INLINE       static __attribute__((always_inline)) inline
#else
#   define INLINE       static inline
#endif

#ifndef __cplusplus
#define false   0
#define true    (!false)
typedef uint32_t    bool;
#endif

typedef struct VM       VM;
typedef struct Process  Process;

// these are made as defines because in ISO the enum values are limited to 0x7FFFFFFF
#define OP_VALUE        0x00000000
#define OP_CALL         0x80000000
#define OP_CALL_MASK    0x7FFFFFFF

#define MAX_TOKEN_SIZE  1023

typedef struct {
    uint32_t        insOffset;
    uint32_t        insCount;
} InterpFunction;

typedef void (*NativeFunction)(Process* proc);

typedef enum {
    FT_INTERP   = 0,
    FT_NATIVE   = 1,
} FunctionType;

typedef struct {
    FunctionType    type;
    bool            isImmediate;
    uint32_t        nameOffset;
    uint32_t        inVS;           // numner of input values to pop from the value stack
    uint32_t        outVS;          // number of output values to push to the value stack
    union {
        InterpFunction  interp;
        NativeFunction  native;
    } u;
} Function;

typedef struct {
    uint32_t        fp;     // function pointer
    uint32_t        ip;     // next instruction address
    uint32_t        lp;     // last local value stack address
} Return;

typedef union {
    bool            b;
    char            c;
    uint8_t         u8;
    uint16_t        u16;
    uint32_t        u32;
    uint64_t        u64;
    int8_t          i8;
    int16_t         i16;
    int32_t         i32;
    int64_t         i64;
    float           f32;
    double          f64;
    void*           ref;
} Value;

typedef enum {
    SM_RO,
    SM_WO,
    SM_RW,
} STREAM_MODE;

typedef struct {
    atomic_uint     refCount;   // should be incremented/decremented atomically
    STREAM_MODE     mode;
    FILE*           file;
} Stream;

typedef struct {
    uint32_t        charCount;
    uint32_t        charCap;
    char*           chars;
    uint32_t        stringCount;
    uint32_t        stringCap;
    uint32_t*       strings;
} StringStack;

typedef union {
    uint32_t        all;
    struct {
        bool            vsOF    : 1;    // value stack overflow flag
        bool            vsUF    : 1;    // value stack underflow flag
        bool            rsOF    : 1;    // return stack overflow flag
        bool            rsUF    : 1;    // return stack underflow flag
        bool            fnOF    : 1;    // function count overflow flag
        bool            insOF   : 1;    // instruction count overflow flag
        bool            chOF    : 1;    // character segment overflow flag
    } indiv;
} ExceptFlags;

typedef struct {
    uint32_t        funcId;     // function index
    uint32_t        ciStart;    // compiler instruction start
} CompiledFunctionEntry;


struct Process {
    VM*             vm;         // root VM
    Process*        prev;       // previous process in the list
    Process*        next;       // next process in the list

    uint32_t        vsCount;
    uint32_t        vsCap;
    uint64_t*       vsObjBits;  // value stack object bits
    Value*          vs;         // value stack

    uint32_t        lsCount;
    uint32_t        lsCap;
    uint64_t*       lsObjBits;  // local object bits
    Value*          ls;         // local stack

    uint32_t        rsCount;
    uint32_t        rsCap;
    Return*         rs;         // return stack

    StringStack     ss;         // string stack

    uint32_t        fp;         // current executing function
    uint32_t        ip;         // pointer to the next instruction to fetch
    uint32_t        lp;         // local stack pointer

    ExceptFlags     exceptFlags;

    struct {
        bool            isTail;
        uint32_t        opcode;
        bool            doReturn;
    }               fetchState;

    struct {
        uint32_t        operation;
        uint32_t        operand;
        const char*     funcName;
        uint32_t        argCount;
        uint32_t        retCount;
    }               decodeState;

    struct {

        Value           s3;         // 4th arg
        Value           s2;         // 3rd arg
        Value           s1;         // 2nd arg
        Value           s0;         // 1st arg
    }               readState;
};

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

    Process*        procList;   // process list
    Process*        activeProc; // currently active process

    // compiler section
    uint32_t        strmCount;
    uint32_t        strmCap;
    Stream**        strms;      // stream stack

    struct {
        uint32_t        cfsCount;   // compiled function stack count
        uint32_t        cfsCap;
        CompiledFunctionEntry*  cfs;

        uint32_t        cisCount;   // compiler instruction count
        uint32_t        cisCap;
        uint32_t*       cis;
    }               compilerState;
};

#define ABORT_ON_EXCEPTIONS()       { if( proc->exceptFlags.all ) { return; } }
#define ABORT_ON_EXCEPTIONS_V(V)    { if( proc->exceptFlags.all ) { return V; } }

#define STOP_IF(FLAG, COND)     { \
        ABORT_ON_EXCEPTIONS() \
        if( (vm->flags.exceptFlags.indiv.FLAG = COND) ) { return; } } \
    }

typedef struct {
    const char*     name;
    bool            isImmediate;
    NativeFunction  native;
    uint32_t        inCount;
    uint32_t        outCount;
} NativeFunctionEntry;


void        vmPushValue     (Process* proc, Value v);
Value       vmPopValue      (Process* proc);
void        vmPushReturn    (Process* proc);
void        vmPopReturn     (Process* proc);
/// pushes a string on the string stack and the string index on the value stack
void        vmPushString    (Process* proc, const char* str);

/// pop the string from the string stack (the value stack remains intact)
void        vmPopString     (Process* proc);

/// return the top string index
uint32_t    vmTopString     (Process* proc);

void        vmPushInstruction   (VM* vm, uint32_t opcode);
void        vmPopInstruction(VM* vm);
void        vmPushCompilerInstruction   (VM* vm, uint32_t opcode);
void        vmPopCompilerInstruction    (VM* vm);

//
// Some day, these will be the ISA for a soft-fpga microcontroller
//
//typedef enum {
//    ARITH           = 0x00,
//    LOGIC           = 0x01,
//    COMP            = 0x02,
//    CONTROL         = 0x03,
//    LOAD_STORE      = 0x04,
//
// } OP_TYPE;
//


Stream*     vmStreamOpenFile(VM* vm, const char* name, STREAM_MODE mode);
Stream*     vmStreamFromFile(VM* vm, FILE* f, STREAM_MODE mode);
Stream*     vmStreamMemory  (VM* vm, uint32_t maxSize);
void        vmStreamPush    (VM* vm, Stream* strm);
void        vmStreamPop     (VM* vm);
uint32_t    vmStreamReadChar(VM* vm, Stream* strm);
bool        vmStreamIsEOS   (VM* vm, Stream* strm);
void        vmStreamWriteChar(VM* vm, Stream* strm, uint32_t ch);
uint32_t    vmStreamSize    (VM* vm, Stream* strm);
uint32_t    vmStreamPos     (VM* vm, Stream* strm);
void        vmStreamSetPos  (VM* vm, Stream* strm, uint32_t pos);

//
// start looking for the function from the last added entry
// Return: 0        -> not found
//         v != 0   -> function index + 1 (decrement to get the function)
//
uint32_t    vmFindFunction          (VM* vm, const char* str);
uint32_t    vmAllocateInterpFunction(VM* vm, const char* str);
uint32_t    vmAddNativeFunction     (VM* vm, const char* str, bool isImmediate, NativeFunction native, uint32_t inVS, uint32_t outVS);

typedef enum {
    CS_NO_ERROR,
    CS_ERROR,
} COMPILATION_STATE;

COMPILATION_STATE   vmCompileString(VM* vm, const char* str);

void        vmRegisterStdWords  (VM* vm);

void        vmSetCall       (Process* proc, uint32_t word);
void        vmSetTailCall   (Process* proc, uint32_t word);

void        vmFetch         (Process* proc);
void        vmExecute       (Process* proc);
void        vmNext          (Process* proc);

void        vmReadEvalPrintLoop (Process* proc);
void        vmLoad          (Process* proc, const char* stream);

Process*    vmProcessList   (VM* vm);


typedef struct {
    uint32_t    maxFunctionCount;       // max function count
    uint32_t    maxInstructionCount;    // max instruction count
    uint32_t    maxCharSegmentSize;     // max const char segment size

    uint32_t    maxFileCount;           // maximum file count (file stack)

    // compiler section
    uint32_t    maxCFCount;             // maximum compiler function count
    uint32_t    maxCISCount;            // maximum compiler instruction count
} VMParameters;

VM*         vmNew       (const VMParameters* params);
Process*    vmNewProcess(VM* vm, uint32_t maxValueCount, uint32_t maxLocalCount, uint32_t maxReturnCount, uint32_t maxCharCount, uint32_t maxStringCount);
void        vmReleaseProcess    (Process* proc);
void        vmRelease   (VM* vm);




