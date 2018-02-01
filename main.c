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

#include "ncvm.h"

int
main(int argc, char* argv[]) {
    fprintf(stdout, "nano Combinator VM \"nCVM\" 2017(c) Wael El Oraiby.\n");

    VMParameters    params = {
        .maxFunctionCount       = 4096,     // max function count
        .maxInstructionCount    = 65536,    // max instruction count
        .maxCharSegmentSize     = 65536,    // max const char segment size
        .maxFileCount           = 1024,     // maximum file count (file stack)
        .maxCFCount             = 64,       // maximum compiler function count
        .maxCISCount            = 65536,    // maximum compiler instruction count
    };

    VM* vm = vmNew(&params);
    Process* proc   = vmNewProcess(vm, 1024, 1024, 1024, 2 * 65536, 32769);

    vmLoad(proc, "bootstrap.ncvm");

    vmPushValue(proc, (Value){ .u32 = 1 });
    vmReadEvalPrintLoop(proc);
    vmReleaseProcess(proc);
    vmRelease(vm);
    return 0;
}
