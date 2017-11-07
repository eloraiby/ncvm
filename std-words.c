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
    const char* orig = buff;
    while(isDigit(*buff)) { ++buff; }
    return *buff == '\0' && buff != orig;
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

    assert(vm->strmCount > 0);
    Stream* strm    = vm->strms[vm->strmCount - 1];

    while( l < maxSize && !isSpace(ch = (char)readChar(vm)) && !vmStreamIsEOS(vm, strm)) {
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
startFuncCompilation(VM* vm) {
    assert(vm->compilerState.cfsCount < vm->compilerState.cfsCap);

    char    token[MAX_TOKEN_SIZE + 1] = { 0 };
    readToken(vm, MAX_TOKEN_SIZE, token);

    vm->compilerState.cfs[vm->compilerState.cfsCount].funcId    = vmAllocateInterpFunction(vm, token);
    vm->compilerState.cfs[vm->compilerState.cfsCount].ciStart   = vm->compilerState.cisCount;

    ++vm->compilerState.cfsCount;
}

static
void
startMacroCompilation(VM* vm) {
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
finishFuncCompilation(VM* vm) {
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
readString(VM* vm) {
    uint32_t    strStartIdx = vm->ss.charCount;
    uint32_t    strIdx      = vm->ss.stringCount;
    int         ch      = 0;
    while( (ch = readChar(vm)) != '"' ) {
        vm->ss.chars[vm->ss.charCount]    = (char)ch;
        ++vm->ss.charCount;
    }

    assert(vm->ss.stringCount < vm->ss.stringCap);
    vm->ss.strings[vm->ss.stringCount]  = strStartIdx;
    ++vm->ss.stringCount;

    vmPushValue(vm, strIdx);
}

static
void
readCommentLine(VM* vm) {
    int         ch      = 0;
    do {
        ch = readChar(vm);
    } while( ch != '\n' && ch != '\a' );
}

static
void
wordAddress(VM* vm) {
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
listWords(VM* vm) {
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
see(VM* vm) {
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
quit(VM* vm) {
    vm->quit    = true;
}

static
void
printInt(VM* vm) {
    uint32_t    v   = vmPopValue(vm);
    fprintf(stdout, "%u", v);
}


void
vmReadEvalPrintLoop(VM* vm) {
    uint32_t    writeToConsole  = vmPopValue(vm);

    if( writeToConsole ) {
        fprintf(stdout, "\n> ");
    }

    bool isEOS  = false;
    while(!vm->quit && !isEOS ) {
        char token[1024]  = { 0 };

        assert(vm->strmCount > 0);
        Stream* strm    = vm->strms[vm->strmCount - 1];

        int ch = readToken(vm, 1023, token);
        isEOS = vmStreamIsEOS(vm, strm);

        if(strlen(token) == 0) {
            continue;
        }

        uint32_t    wordId   = vmFindFunction(vm, token);
        if( wordId == 0 ) {
            if( isInt(token) ) { // push the value
                uint32_t    value = tokToInt(token);
                if( isInCompileMode(vm) ) {
                    vmPushCompilerInstruction(vm, OP_CALL_MASK & value);
                } else {
                    vmPushValue(vm, value);
                }
            } else {
                fprintf(stderr, "Error: word %s not found in dictionnary\n", token);
            }
        } else {
            if( isInCompileMode(vm) && !vm->funcs[wordId - 1].isImmediate ) {
                vmPushCompilerInstruction(vm, OP_CALL | (wordId - 1));
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
            }
        }

        if( ch == '\n' && writeToConsole ) {
            fprintf(stdout, "\n> ");
        }
    }
}

#define ALL 0xFFFFFFFF  /* mostly used for immediates/macros    */

static
void
load(VM* vm) {
    uint32_t    strIdx  = vmPopValue(vm);
    uint32_t    strStart= vm->ss.strings[strIdx];
    const char* fName   = &vm->ss.chars[strStart];
    Stream*     strm    = vmStreamOpenFile(vm, fName, SM_RO);

    vmStreamPush(vm, strm);
    vmPushValue(vm, 0);
    vmReadEvalPrintLoop(vm);
    vmStreamPop(vm);
    vmPopString(vm);
}

static
void
startLambda(VM* vm) {
    assert(vm->compilerState.cfsCount < vm->compilerState.cfsCap);

    char    token[MAX_TOKEN_SIZE + 1] = { 0 };
    sprintf(token, "lambda#%d", vm->insCount);

    vm->compilerState.cfs[vm->compilerState.cfsCount].funcId    = vmAllocateInterpFunction(vm, token);
    vm->compilerState.cfs[vm->compilerState.cfsCount].ciStart   = vm->compilerState.cisCount;

    ++vm->compilerState.cfsCount;
}

static
const NativeFunctionEntry entries[]  = {
    { "repl",       false,  vmReadEvalPrintLoop,        ALL,    ALL },

    { ":",          true,   startFuncCompilation,       ALL,    ALL },
    { "!",          true,   startMacroCompilation,      ALL,    ALL },
    { ";",          true,   finishFuncCompilation,      ALL,    ALL },
    { "\"",         true,   readString,                 ALL,    ALL },
    { "//",         true,   readCommentLine,            0,      0   },
    { "@",          true,   wordAddress,                ALL,    ALL },
    { "\\",         true,   startLambda,                ALL,    ALL },

    { ".i",         false,  printInt,                   1,      0   },
    { "lsws",       false,  listWords,                  0,      0   },
    { "lsvs",       false,  listValues,                 0,      0   },
    { "see",        false,  see,                        1,      0   },

    { "load",       false,  load,                       1,      0   },

    { "quit",       false,  quit,                       0,      0   },
};

void
vmRegisterStdWords(VM* vm) {
    for(uint32_t i = 0; i < sizeof(entries) / sizeof(NativeFunctionEntry); ++i) {
        vmAddNativeFunction(vm, entries[i].name, entries[i].isImmediate, entries[i].native, entries[i].inCount, entries[i].outCount);
    }
}


