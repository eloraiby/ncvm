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


static inline
void
vmPushValue(VM* vm, uint32_t v) {
    assert(vm->vsCount < vm->vsCap);
    vm->vs[vm->vsCount] = v;
    ++vm->vsCount;
}

static inline
uint32_t
vmPopValue(VM* vm) {
    assert(vm->vsCount != 0);
    --vm->vsCount;
    return vm->vs[vm->vsCount];
}

static inline
void
vmPushReturn(VM* vm) {
    Return r = { .fp = vm->fp, .ip = vm->ip };
    vm->rs[vm->rsCount] = r;
    ++vm->rsCount;
}

static inline
void
vmPopReturn(VM* vm) {
    --vm->rsCount;
    Return  r   = vm->rs[vm->rsCount];
    vm->fp  = r.fp;
    vm->ip  = r.ip;
}

static inline
uint32_t
vmGetOperation(uint32_t opcode) {
    return opcode & 0x80000000;
}

static inline
uint32_t
vmGetOperand(uint32_t opcode) {
    return opcode & 0x7FFFFFFF;
}

static
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

static
void
vmPopString(VM* vm) {
    assert(vm->ss.charCount > 0);
    assert(vm->ss.stringCount > 0);
    vm->ss.charCount    = vm->ss.strings[vm->ss.stringCount - 1];
    --vm->ss.stringCount;
}

static inline
void
vmPushInstruction(VM* vm, uint32_t opcode) {
    assert(vm->insCount < vm->insCap);
    vm->ins[vm->insCount++] = opcode;
}

static inline
void
vmPopInstruction(VM* vm) {
    assert(vm->insCount > 0);
    --vm->insCount;
}

static inline
void
vmPushCompilerInstruction(VM* vm, uint32_t opcode) {
    assert(vm->cisCount < vm->cisCap);
    vm->cis[vm->cisCount++] = opcode;
}

static inline
void
vmPopCompilerInstruction(VM* vm) {
    assert(vm->cisCount > 0);
    --vm->cisCount;
}

void
vmNext(VM* vm) {
    uint32_t    ip      = vm->ip;
    uint32_t    fp      = vm->fp;

    assert(fp < vm->funcCount);

    const char* fName   = &vm->chars[vm->funcs[fp].nameOffset];
    log("in %s - %d | %d :", fName, fp, ip);

    Function    func    = vm->funcs[fp];
    assert(func.type == FT_INTERP);

    uint32_t    fpInsCount  = func.u.interp.insCount;

    if( ip >= fpInsCount ) {
        assert(ip == fpInsCount);
        log("\tret - ");
        vmPopReturn(vm);    // ip exceeds instruction count, return
        log("%d:%d | %d\n", vm->fp, vm->ip, vm->rsCount);
    } else {
        uint32_t*   ins     = &vm->ins[func.u.interp.insOffset];
        uint32_t    opcode  = ins[ip];
        ++vm->ip;

        bool        pushReturn  = false;

        if( vm->ip < fpInsCount ) {
            pushReturn  = true;
        }

        uint32_t    operation   = vmGetOperation(opcode);
        uint32_t    operand     = vmGetOperand(opcode);

        switch( operation ) {
        case OP_VALUE:  // value
            log("\t%u\n", operand);
            vmPushValue(vm, operand);
            break;
        case OP_CALL:   // call
            if( pushReturn ) {
                log("\tcall ");
                vmPushReturn(vm);   // normal call: push return value
            } else {
                log("\ttail ");
            }

            fName   = &vm->chars[vm->funcs[operand].nameOffset];
            log("[%d]\t%s ", vm->rsCount, fName);

            switch(vm->funcs[operand].type) {
            case FT_NATIVE:
                log("<NAT>\n");
                vm->funcs[operand].u.native(vm);
                break;
            case FT_INTERP:
                log("<INT>\n");
                vm->fp  = operand;
                vm->ip  = 0;
                break;
            }

            break;
        }
    }
}

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
    Function    f   = { .type = FT_INTERP, .isImmediate = false, .nameOffset = addConstString(vm, str), .u = { .interp = { .insOffset = 0, .insCount = 0 } } };
    uint32_t    fidx    = vm->funcCount;
    vm->funcs[vm->funcCount]    = f;
    ++vm->funcCount;
    return fidx;
}

uint32_t
vmAddNativeFunction(VM* vm, const char* str, bool isImmediate, NativeFunction native) {
    Function    f   = { .type = FT_NATIVE, .isImmediate = isImmediate, .nameOffset = addConstString(vm, str), .u = { .native = native } };
    uint32_t    fidx    = vm->funcCount;
    vm->funcs[vm->funcCount]    = f;
    ++vm->funcCount;
    return fidx;
}

static
void
vmPrintInt(VM* vm) {
    uint32_t val = vmPopValue(vm);
    printf("%u", val);
}

static inline
bool
isSpace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\a';
}

static inline
bool
isDigit(char ch) {
    return (ch >= '0' && ch <= '9');
}

static inline
bool
isInt(char* buff) {
    while(isDigit(*buff)) { ++buff; }
    return *buff == '\0';
}

static inline
int
readChar(VM* vm) {
    assert(vm->fsCount > 0);
    return fgetc(vm->fs[vm->fsCount - 1]);
}

static
int
readToken(VM* vm, int maxSize, char* tmp) {
    char    ch = 0;
    int     l  = 0;
    while( l < maxSize && !isSpace(ch = (char)readChar(vm))) {
        tmp[l++] = ch;
    }
    return ch;
}

static
uint32_t
tokToInt(char* buff) {
    uint32_t    value   = 0;
    while(*buff) {
        value   = value * 10 + (*buff - '0');
        ++buff;
    }
    return value;
}

static inline
bool
isInCompileMode(VM* vm) {
    return vm->cfsCount > 0;
}

static
void
decompileOpcode(VM* vm, uint32_t opcode) {
    switch(opcode & OP_CALL) {
    case OP_VALUE:
        fprintf(stdout, "\t%u\n", opcode);
        break;
    case OP_CALL:
        fprintf(stdout, "\t%s\n", &vm->chars[vm->funcs[opcode & 0x7FFFFFFF].nameOffset]);
        break;
    }
}

static
void
vmStartFuncCompilation(VM* vm) {
    assert(vm->cfsCount < vm->cfsCap);

    char    token[MAX_TOKEN_SIZE + 1] = { 0 };
    readToken(vm, MAX_TOKEN_SIZE, token);

    vm->cfs[vm->cfsCount].funcId    = vmAllocateInterpFunction(vm, token);
    vm->cfs[vm->cfsCount].ciStart   = vm->cisCount;

    ++vm->cfsCount;
}

static
void
vmStartMacroCompilation(VM* vm) {
    assert(vm->cfsCount < vm->cfsCap);

    char    token[MAX_TOKEN_SIZE + 1] = { 0 };
    readToken(vm, MAX_TOKEN_SIZE, token);

    uint32_t    funcId  = vmAllocateInterpFunction(vm, token);
    vm->funcs[funcId].isImmediate    = true;

    vm->cfs[vm->cfsCount].funcId    = funcId;
    vm->cfs[vm->cfsCount].ciStart   = vm->cisCount;

    ++vm->cfsCount;
}


static
void
vmFinishFuncCompilation(VM* vm) {
    assert(vm->cfsCount > 0);

    uint32_t    funcId      = vm->cfs[vm->cfsCount - 1].funcId;

    log("finish %s (%d):\n", &vm->chars[vm->funcs[funcId].nameOffset], funcId);

    uint32_t    insCount    = 0;
    uint32_t    insOffset   = vm->insCount;

    for( uint32_t ci = vm->cfs[vm->cfsCount - 1].ciStart; ci < vm->cisCount; ++ci) {
        ++insCount;
        vmPushInstruction(vm, vm->cis[ci]);
        decompileOpcode(vm, vm->cis[ci]);
    }

    vm->cisCount    = vm->cfs[vm->cfsCount - 1].ciStart;
    vm->funcs[funcId].u.interp.insOffset  = insOffset;
    vm->funcs[funcId].u.interp.insCount   = insCount;
    --vm->cfsCount;
}

static
void
vmReadString(VM* vm) {
    uint32_t    strIdx  = vm->charCount;
    int         ch      = 0;
    while( (ch = readChar(vm)) != '"' ) {
        vm->chars[vm->charCount]    = (char)ch;
        ++vm->charCount;
    }

    vmPushValue(vm, strIdx);
}

static
void
vmWordAddress(VM* vm) {
    char    token[MAX_TOKEN_SIZE + 1] = { 0 };
    readToken(vm, MAX_TOKEN_SIZE, token);
    uint32_t funcId = vmFindFunction(vm, token);
    assert(funcId != 0);
    if( isInCompileMode(vm) ) {
        vmPushCompilerInstruction(vm, funcId - 1);
    } else {
        vmPushValue(vm, funcId - 1);
    }
}

static
void
vmDup(VM* vm) {
    uint32_t    v   = vmPopValue(vm);
    vmPushValue(vm, v);
    vmPushValue(vm, v);
}

static
void
vmAddUInt(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vmPushValue(vm, a + b);
}

static
void
vmSubUInt(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vmPushValue(vm, a - b);
}

static
void
vmMulUInt(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vmPushValue(vm, a * b);
}

static
void
vmDivUInt(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vmPushValue(vm, a / b);
}

static
void
vmModUInt(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vmPushValue(vm, a % b);
}


static
void
vmCond(VM* vm) {
    uint32_t    elseExp = vmPopValue(vm);
    uint32_t    thenExp = vmPopValue(vm);
    uint32_t    boolExp = vmPopValue(vm);

    if( boolExp != 0 ) {
        vm->fp  = thenExp;
    } else {
        vm->fp  = elseExp;
    }

    vm->ip = 0;
}

static
void
vmIntEq(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vmPushValue(vm, a == b);
}


static
void
vmIntNotEq(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vmPushValue(vm, a != b);
}


static
void
vmListWords(VM* vm) {
    for( uint32_t f = 0; f < vm->funcCount; ++f ) {
        fprintf(stdout, "%d - %s\n", f, &vm->chars[vm->funcs[f].nameOffset]);
    }
}


static
void
vmSee(VM* vm) {
    char    token[MAX_TOKEN_SIZE + 1] = { 0 };
    readToken(vm, MAX_TOKEN_SIZE, token);

    uint32_t    funcId  = vmFindFunction(vm, token);

    switch(funcId) {
    case 0:
        fprintf(stdout, "word %s doesn't exist\n", token);
        break;
    default:
        fprintf(stdout, "%d - %s:\n", funcId - 1, &vm->chars[vm->funcs[funcId - 1].nameOffset]);
        switch(vm->funcs[funcId - 1].type) {
        case FT_NATIVE:
            fprintf(stderr, "\t<native>\n");
            break;
        case FT_INTERP:
            for(uint32_t i = 0; i < vm->funcs[funcId - 1].u.interp.insCount; ++i ) {
                uint32_t    opcodeOffset    = vm->funcs[funcId - 1].u.interp.insOffset + i;
                uint32_t    opcode  = vm->ins[opcodeOffset];
                decompileOpcode(vm, opcode);
            }
            break;
        }
    }
}

static
void
vmQuit(VM* vm) {
    vm->quit    = true;
}


static
void
vmReadEvalPrintLoop(VM* vm) {
    fprintf(stdout, "\n> ");

    while(!vm->quit) {
        char token[1024]  = { 0 };
        int ch = readToken(vm, 1023, token);

        uint32_t    wordId   = vmFindFunction(vm, token);
        if( wordId == 0 ) {
            if( isInt(token) ) { // push the value
                uint32_t    value = tokToInt(token);
                if( isInCompileMode(vm) ) {
                    vmPushCompilerInstruction(vm, 0x7FFFFFFF & value);
                } else {
                    vmPushValue(vm, value);
                }
            } else {
                fprintf(stderr, "Error: word %s not found in dictionnary\n", token);
            }
        } else {
            if( isInCompileMode(vm) && !vm->funcs[wordId - 1].isImmediate ) {
                vmPushCompilerInstruction(vm, 0x80000000 | (wordId - 1));
            } else {
                switch( vm->funcs[wordId - 1].type ) {
                case FT_INTERP: {
                    uint32_t    origRetCount    = vm->rsCount;
                    vm->fp  = 0;
                    vm->ip  = 0;
                    vmPushReturn(vm);
                    log("starting with depth: %d\n", origRetCount);

                    vm->fp  = wordId - 1;
                    vm->ip  = 0;
                    while(!vm->quit && vm->rsCount > origRetCount) {
                        vmNext(vm);
                    }
                } break;
                case FT_NATIVE:
                    vm->funcs[wordId - 1].u.native(vm);
                    break;
                }
            }
        }

        if( ch == '\n' ) {
            fprintf(stdout, "\n> ");
        }
    }
}


static
const NativeFunctionEntry entries[]  = {
    { "repl",       false,  vmReadEvalPrintLoop     },  // should always be @0

    { ":",          true,   vmStartFuncCompilation  },
    { "!",          true,   vmStartMacroCompilation },
    { ";",          true,   vmFinishFuncCompilation },
    { ".i",         true,   vmPrintInt              },
    { "\"",         true,   vmReadString            },
    { "@",          true,   vmWordAddress           },

    { "lsw",        false,  vmListWords             },
    { "see",        false,  vmSee                   },

    { "dup",        false,  vmDup                   },
    { "+",          false,  vmAddUInt               },
    { "-",          false,  vmSubUInt               },
    { "*",          false,  vmMulUInt               },
    { "/",          false,  vmDivUInt               },
    { "%",          false,  vmModUInt               },

    { "=",          false,  vmIntEq                 },
    { "<>",         false,  vmIntNotEq              },

    { "cond",       false,  vmCond                  },

    { "quit",       false,  vmQuit                  },

};

VM*
vmNew(uint32_t maxFunctionCount,
      uint32_t maxInstructionCount,
      uint32_t maxCharSegmentSize,
      uint32_t maxValuesCount,
      uint32_t maxReturnCount,
      uint32_t maxFileCount,
      uint32_t maxSSCharCount,
      uint32_t maxSSStringCount,
      uint32_t maxCFCount,
      uint32_t maxCISCount)
{
    VM* vm  = (VM*)malloc(sizeof(VM));
    memset(vm, 0, sizeof(VM));

    vm->funCap  = maxFunctionCount;
    vm->insCap  = maxInstructionCount;
    vm->charCap = maxCharSegmentSize;
    vm->vsCap   = maxValuesCount;
    vm->rsCap   = maxReturnCount;
    vm->fsCap   = maxFileCount;

    vm->funcs   = (Function*)malloc(maxFunctionCount * sizeof(Function));
    memset(vm->funcs, 0, maxFunctionCount * sizeof(Function));

    vm->ins     = (uint32_t*)malloc(maxInstructionCount * sizeof(uint32_t));
    memset(vm->ins, 0, maxInstructionCount * sizeof(uint32_t));

    vm->chars   = (char*)malloc(maxCharSegmentSize);
    memset(vm->chars, 0, maxCharSegmentSize);

    vm->vs      = (uint32_t*)malloc(maxValuesCount * sizeof(uint32_t));
    memset(vm->vs, 0, maxValuesCount * sizeof(uint32_t));

    vm->rs      = (Return*)malloc(maxReturnCount * sizeof(Return));
    memset(vm->rs, 0, maxReturnCount * sizeof(Return));

    vm->fs      = (FILE**)malloc(maxFileCount * sizeof(FILE*));
    memset(vm->fs, 0, maxFileCount * sizeof(FILE*));

    vm->fs[0]   = stderr;
    vm->fs[1]   = stdout;
    vm->fs[2]   = stdin;
    vm->fsCount = 3;

    vm->ss.chars    = (char*)malloc(maxSSCharCount);
    memset(vm->ss.chars, 0, maxSSCharCount);
    vm->ss.charCap  = maxSSCharCount;

    vm->ss.strings  = (uint32_t*)malloc(maxSSStringCount * sizeof(uint32_t));
    memset(vm->ss.strings, 0, maxSSStringCount * sizeof(uint32_t));
    vm->ss.stringCap    = maxSSStringCount;

    vm->cfs     = (CompiledFunctionEntry*)malloc(maxCFCount * sizeof(CompiledFunctionEntry));
    memset(vm->cfs, 0, maxCFCount * sizeof(CompiledFunctionEntry));
    vm->cfsCap  = maxCFCount;

    vm->cis     = (uint32_t*)malloc(maxCISCount * sizeof(uint32_t));
    memset(vm->cis, 0, maxCISCount * sizeof(uint32_t));
    vm->cisCap  = maxCISCount;

    for(uint32_t i = 0; i < sizeof(entries) / sizeof(NativeFunctionEntry); ++i) {
        vmAddNativeFunction(vm, entries[i].name, entries[i].isImmediate, entries[i].native);
    }

    return vm;
}

void
vmRelease(VM* vm) {
    free(vm->funcs);
    free(vm->ins);
    free(vm->chars);
    free(vm->vs);
    free(vm->rs);
    free(vm->fs);
    free(vm->ss.chars);
    free(vm->ss.strings);
    free(vm->cfs);
    free(vm->cis);
    free(vm);
}


int
main(int argc, char* argv[]) {
    printf("nanoforth 2017(c) Wael El Oraiby.\n");

    VM* vm = vmNew(4096, 65536, 65536, 1024, 1024, 1024, 2 * 65536, 32768, 64, 65536);

    vmReadEvalPrintLoop(vm);

    vmRelease(vm);
    return 0;
}
