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
vmAddNativeFunction(VM* vm, const char* str, bool isImmediate, NativeFunction native) {
    Function    f   = {
        .type           = FT_NATIVE,
        .isImmediate    = isImmediate,
        .nameOffset     = addConstString(vm, str),
        .u              = { .native = native }
    };

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

        if( operation == OP_VALUE ) {
            log("\t%u\n", operand);
            vmPushValue(vm, operand);
        } else { // OP_CALL
            if( pushReturn ) {
                log("\tcall ");
                vmPushReturn(vm);   // normal call: push return value
            } else {
                log("\ttail ");
            }

            fName   = &vm->chars[vm->funcs[operand].nameOffset];
            log("[%d]\t%s ", vm->rsCount, fName);

            if(vm->funcs[operand].type == FT_NATIVE) {
                log("<NAT>\n");
                vm->funcs[operand].u.native(vm);
                if( pushReturn ) {
                    log("\tret<NAT>\n");
                    vmPopReturn(vm);
                }
            } else {    // FT_INTERP
                log("<INT>\n");
                vm->fp  = operand;
                vm->ip  = 0;
            }
        }
    }
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

    vm->cfs             = (CompiledFunctionEntry*)calloc(params->maxCFCount, sizeof(CompiledFunctionEntry));
    vm->cfsCap          = params->maxCFCount;

    vm->cis             = (uint32_t*)calloc(params->maxCISCount, sizeof(uint32_t));
    vm->cisCap          = params->maxCISCount;

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
    free(vm->cfs);
    free(vm->cis);
    free(vm);
}
