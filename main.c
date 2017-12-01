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

int
main(int argc, char* argv[]) {
    fprintf(stdout, "nano Combinator VM \"nCVM\" 2017(c) Wael El Oraiby.\n");

    VMParameters    params = {
        .maxFunctionCount       = 4096,     // max function count
        .maxInstructionCount    = 65536,    // max instruction count
        .maxCharSegmentSize     = 65536,    // max const char segment size
        .maxValuesCount         = 1024,     // maximum value count (value stack)
        .maxLocalsCount         = 1024,     // maximum local count (local stack)
        .maxReturnCount         = 1024,     // maximum return count (return stack)
        .maxFileCount           = 1024,     // maximum file count (file stack)
        .maxSSCharCount         = 2 * 65536,// maximum stacked char count (char stack for string stack)
        .maxSSStringCount       = 32768,    // maximum stacked string count (string stack)
        .maxCFCount             = 64,       // maximum compiler function count
        .maxCISCount            = 65536,    // maximum compiler instruction count
    };

    VM* vm = vmNew(&params);

    vmLoad(vm, "bootstrap.ncvm");

    vmPushValue(vm, (Value){ .u32 = 1 });
    vmReadEvalPrintLoop(vm);

    vmRelease(vm);
    return 0;
}
