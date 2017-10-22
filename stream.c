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

#include <stdatomic.h>
#include "nanoforth.h"

Stream*
vmStreamOpenFile(VM* vm, const char* name, STREAM_MODE mode) {
    ABORT_ON_EXCEPTIONS_V(NULL)
    const char* m   = mode == SM_RO ? "rb" : (mode == SM_RW ? "rw" : (mode == SM_WO ? "wb" : "rb"));

    FILE*   f   = fopen(name, m);
    if(f) {
        Stream* strm    = (Stream*)calloc(1, sizeof(Stream));
        atomic_store(&strm->refCount,   0);
        atomic_store(&strm->pos,        0);
        strm->mode      = mode;
        strm->ty        = ST_FILE;
        strm->stream.file   = f;
        return strm;
    } else {
        return NULL;
    }
}

Stream*
vmStreamFromFile(VM* vm, FILE* f, STREAM_MODE mode) {
    ABORT_ON_EXCEPTIONS_V(NULL)
    if(f) {
        Stream* strm    = (Stream*)calloc(1, sizeof(Stream));
        atomic_store(&strm->refCount,   0);
        atomic_store(&strm->pos,        0);
        strm->mode      = mode;
        strm->ty        = ST_FILE;
        strm->stream.file   = f;
        return strm;
    } else {
        return NULL;
    }
}

Stream*
vmStreamMemory(VM* vm, uint32_t maxSize) {
    ABORT_ON_EXCEPTIONS_V(NULL)
    char*   m       = (char*)calloc(maxSize, 1);
    Stream* strm    = (Stream*)calloc(1, sizeof(Stream));
    atomic_store(&strm->refCount,   0);
    atomic_store(&strm->pos,        0);
    strm->mode      = SM_RW;
    strm->ty        = ST_MEM;
    strm->stream.memory.address = m;
    strm->stream.memory.size    = maxSize;
    return NULL;
}

void
vmStreamPush(VM* vm, Stream* strm) {
    ABORT_ON_EXCEPTIONS()
    atomic_fetch_add(&strm->refCount, 1);
    vm->strms[vm->strmCount]   = strm;
    ++vm->strmCount;
}

void
vmStreamPop(VM* vm, Stream* strm) {
    ABORT_ON_EXCEPTIONS()
    if( strm->refCount != 0 ) {
        atomic_fetch_sub(&strm->refCount, 1);
        if( strm->refCount == 0 ) {
            switch(strm->ty) {
            case ST_FILE:
                fclose(strm->stream.file);
                break;
            case ST_MEM:
                free(strm->stream.memory.address);
                break;
            }
        }
    }
}

uint32_t    vmStreamReadChar(VM* vm, Stream* strm);
uint32_t    vmStreamPeekChar(VM* vm, Stream* strm);
void        vmStreamIsEOS   (VM* vm, Stream* strm);
void        vmStreamWriteChar(VM* vm, Stream* strm, uint32_t ch);
uint32_t    vmStreamSize    (VM* vm, Stream* strm);
uint32_t    vmStreamPos     (VM* vm, Stream* strm);
void        vmStreamSetPos  (VM* vm, Stream* strm);


