#ifndef NCVM_MODULE__H
#define NCVM_MODULE__H
/*
** Copyright (c) 2017-2018 Wael El Oraiby.
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

#include <stdint.h>
#include <stdbool.h>

typedef struct Process Process;

//
// instruction format:
//
// 0 | XXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX   : Literal
// 1 | MMMMMMM MMMMFFFF FFFFFFFF FFFFFFFF   : function call (M: Module bit, F: Function bit) : 2048 modules, 2^20 functions
//
// Notes:
//  1. module 0x000 refers to the CPU implemented opcode module (HW module)
//  2. module 0x7FF refers to the current module
//

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
    uint32_t    versionMajor;       // module major version
    uint32_t    versionMinor;       // module minor version
    uint32_t    versionRev;         // module revision version

    uint32_t    extModuleCount;     // external module count
    uint32_t    constStringCount;   // constant string count

    uint32_t    extFunctionCount;   // externally referenced function count
    uint32_t    intFunctionCount;   // internally defined function count

    uint32_t    moduleNameId;       // inside the string table

    uint32_t    instructionCount;   // total instruction count

    Function*   intFunctionTable;
    uint32_t*   constStringTable;   // constant string offsets
    uint32_t*   instructions;       // instruction block

} Module;
#endif // NCVM_MODULE__H
