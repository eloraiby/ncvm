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
    assert(vm->strmCount > 0);
    Stream* strm    = vm->strms[vm->strmCount - 1];
    return (int)vmStreamReadChar(vm, strm);
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
    return vm->compilerState.cfsCount > 0;
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
    assert(vm->compilerState.cfsCount < vm->compilerState.cfsCap);

    char    token[MAX_TOKEN_SIZE + 1] = { 0 };
    readToken(vm, MAX_TOKEN_SIZE, token);

    vm->compilerState.cfs[vm->compilerState.cfsCount].funcId    = vmAllocateInterpFunction(vm, token);
    vm->compilerState.cfs[vm->compilerState.cfsCount].ciStart   = vm->compilerState.cisCount;

    ++vm->compilerState.cfsCount;
}

static
void
vmStartMacroCompilation(VM* vm) {
    assert(vm->compilerState.cfsCount < vm->compilerState.cfsCap);

    char    token[MAX_TOKEN_SIZE + 1] = { 0 };
    readToken(vm, MAX_TOKEN_SIZE, token);

    uint32_t    funcId  = vmAllocateInterpFunction(vm, token);
    vm->funcs[funcId].isImmediate    = true;

    vm->compilerState.cfs[vm->compilerState.cfsCount].funcId    = funcId;
    vm->compilerState.cfs[vm->compilerState.cfsCount].ciStart   = vm->compilerState.cisCount;

    ++vm->compilerState.cfsCount;
}


static
void
vmFinishFuncCompilation(VM* vm) {
    assert(vm->compilerState.cfsCount > 0);

    uint32_t    funcId      = vm->compilerState.cfs[vm->compilerState.cfsCount - 1].funcId;

    log("finish %s (%d):\n", &vm->chars[vm->funcs[funcId].nameOffset], funcId);

    uint32_t    insCount    = 0;
    uint32_t    insOffset   = vm->insCount;

    for( uint32_t ci = vm->compilerState.cfs[vm->compilerState.cfsCount - 1].ciStart; ci < vm->compilerState.cisCount; ++ci) {
        ++insCount;
        vmPushInstruction(vm, vm->compilerState.cis[ci]);
        decompileOpcode(vm, vm->compilerState.cis[ci]);
    }

    vm->compilerState.cisCount    = vm->compilerState.cfs[vm->compilerState.cfsCount - 1].ciStart;
    vm->funcs[funcId].u.interp.insOffset  = insOffset;
    vm->funcs[funcId].u.interp.insCount   = insCount;
    --vm->compilerState.cfsCount;
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
vmListWords(VM* vm) {
    for( uint32_t f = 0; f < vm->funcCount; ++f ) {
        fprintf(stdout, "%d - %s : %d : %d\n", f, &vm->chars[vm->funcs[f].nameOffset], vm->funcs[f].inVS, vm->funcs[f].outVS);
    }
}

static
void
listValues(VM* vm) {
    for(uint32_t i = 0; i < vm->vsCount; ++i) {
        fprintf(stdout, "[%d] - 0x%08X\n", i, vm->vs[i]);
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
    uint32_t    v   = vmPopValue(vm);
    printf("%u", v);
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
                uint32_t    origRetCount    = vm->rsCount;
                vm->fp  = 0;
                vm->ip  = 0;
                vmPushReturn(vm);

                vmSetTailCall(vm, wordId - 1);
                vmExecute(vm);
                while(!vm->quit && vm->rsCount > origRetCount) {
                    vmNext(vm);
                }
                /*
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
                */
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
    { "repl",       false,  vmReadEvalPrintLoop,        ALL,    ALL },

    { ":",          true,   vmStartFuncCompilation,     ALL,    ALL },
    { "!",          true,   vmStartMacroCompilation,    ALL,    ALL },
    { ";",          true,   vmFinishFuncCompilation,    ALL,    ALL },
    { "\"",         true,   vmReadString,               ALL,    ALL },
    { "@",          true,   vmWordAddress,              ALL,    ALL },

    { ".i",         false,  vmPrintInt,                 1,      0   },
    { "lsw",        false,  vmListWords,                0,      0   },
    { "lsvs",       false,  listValues,                 0,      0   },
    { "see",        false,  vmSee,                      1,      0   },


    { "quit",       false,  vmQuit,                     0,      0   },

};

void
vmRegisterStdWords(VM* vm) {
    for(uint32_t i = 0; i < sizeof(entries) / sizeof(NativeFunctionEntry); ++i) {
        vmAddNativeFunction(vm, entries[i].name, entries[i].isImmediate, entries[i].native, entries[i].inCount, entries[i].outCount);
    }
}


