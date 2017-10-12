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
                if( pushReturn ) {
                    log("\tret<NAT>\n");
                    vmPopReturn(vm);
                }

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
