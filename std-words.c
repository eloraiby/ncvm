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

INLINE
bool
isSpace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\a';
}

INLINE
bool
isDigit(char ch) {
    return (ch >= '0' && ch <= '9');
}

INLINE
bool
isInt(const char* buff) {
    while(isDigit(*buff)) { ++buff; }
    return *buff == '\0';
}

INLINE
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
vmPeek(VM* vm) {
    uint32_t    addr   = vmPopValue(vm);
    vmPushValue(vm, vm->vs[vm->vsCount - addr - 1]);
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

    if( vm->flags.bf != 0 ) {
        vm->fp  = thenExp;
    } else {
        vm->fp  = elseExp;
    }

    vm->ip = 0;
    vm->flags.bf    = false;    // clear the boolean flag always after test
}

static
void
vmUIntEq(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vm->flags.bf    = (a == b);
}


static
void
vmUIntNotEq(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vm->flags.bf    = (a != b);
}

static
void
vmUIntGEq(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vm->flags.bf    = (a >= b);
}


static
void
vmUIntLEq(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vm->flags.bf    = (a <= b);
}


static
void
vmUIntGT(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vm->flags.bf    = (a > b);
}


static
void
vmUIntLT(VM* vm) {
    uint32_t    b   = vmPopValue(vm);
    uint32_t    a   = vmPopValue(vm);

    vm->flags.bf    = (a < b);
}

static
void
vmBoolFlagValue(VM* vm) {
    vmPushValue(vm, vm->flags.bf);
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
vmPrintInt(VM* vm) {
    uint32_t val = vmPopValue(vm);
    printf("%u", val);
}


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

#define ALL 0xFFFFFFFF  /* mostly used for immediates/macros    */

static
const NativeFunctionEntry entries[]  = {
    { "repl",       false,  vmReadEvalPrintLoop,        ALL,    ALL },  // should always be @0

    { ":",          true,   vmStartFuncCompilation,     ALL,    ALL },
    { "!",          true,   vmStartMacroCompilation,    ALL,    ALL },
    { ";",          true,   vmFinishFuncCompilation,    ALL,    ALL },
    { "\"",         true,   vmReadString,               ALL,    ALL },
    { "@",          true,   vmWordAddress,              ALL,    ALL },

    { ".i",         false,  vmPrintInt,                 1,      0   },
    { "lsw",        false,  vmListWords,                0,      0   },
    { "see",        false,  vmSee,                      1,      0   },

    { "dup",        false,  vmDup,                      1,      2   },
    { "@>",         false,  vmPeek,                     1,      1   },
    { "+",          false,  vmAddUInt,                  2,      1   },
    { "-",          false,  vmSubUInt,                  2,      1   },
    { "*",          false,  vmMulUInt,                  2,      1   },
    { "/",          false,  vmDivUInt,                  2,      1   },
    { "%",          false,  vmModUInt,                  2,      1   },

    { "=",          false,  vmUIntEq,                   2,      0   },
    { "<>",         false,  vmUIntNotEq,                2,      0   },
    { ">=",         false,  vmUIntGEq,                  2,      0   },
    { "<=",         false,  vmUIntLEq,                  2,      0   },
    { ">",          false,  vmUIntGT,                   2,      0   },
    { "<",          false,  vmUIntLT,                   2,      0   },

    { "bf>",        false,  vmBoolFlagValue,            0,      1   },

    { "?",          false,  vmCond,                     2,      0   },

    { "quit",       false,  vmQuit,                     0,      0   },

};

void
vmRegisterStdWords(VM* vm) {
    for(uint32_t i = 0; i < sizeof(entries) / sizeof(NativeFunctionEntry); ++i) {
        vmAddNativeFunction(vm, entries[i].name, entries[i].isImmediate, entries[i].native);
    }
}


