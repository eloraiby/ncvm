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

#include <stdatomic.h>
#include "ncvm.h"

Stream*
vmStreamOpenFile(VM* vm, const char* name, STREAM_MODE mode) {
    //ABORT_ON_EXCEPTIONS_V(NULL)
    const char* m   = mode == SM_RO ? "rb" : (mode == SM_RW ? "wb+" : (mode == SM_WO ? "wb" : "rb"));

    FILE*   f   = fopen(name, m);
    if(f) {
        Stream* strm    = (Stream*)calloc(1, sizeof(Stream));
        atomic_store(&strm->refCount,   0);
        strm->mode      = mode;
        strm->file      = f;
        return strm;
    } else {
        return NULL;
    }
}

Stream*
vmStreamFromFile(VM* vm, FILE* f, STREAM_MODE mode) {
    //ABORT_ON_EXCEPTIONS_V(NULL)
    if(f) {
        Stream* strm    = (Stream*)calloc(1, sizeof(Stream));
        atomic_store(&strm->refCount,   0);
        strm->mode      = mode;
        strm->file      = f;
        return strm;
    } else {
        return NULL;
    }
}

Stream*
vmStreamFromMemory(VM* vm, const char* str, uint32_t size) {
    //ABORT_ON_EXCEPTIONS_V(NULL)
    FILE*   f   = tmpfile();
    if(f) {
        Stream* strm    = (Stream*)calloc(1, sizeof(Stream));
        atomic_store(&strm->refCount,   0);
        fwrite(str, 1, size, f);
        rewind(f);
        strm->mode      = SM_RW;
        strm->file      = f;
        return strm;
    } else {
        return NULL;
    }
}

void
vmStreamPush(VM* vm, Stream* strm) {
    //ABORT_ON_EXCEPTIONS()
    atomic_fetch_add(&strm->refCount, 1);
    vm->strms[vm->strmCount]   = strm;
    ++vm->strmCount;
}

void
vmStreamPop(VM* vm) {
    //ABORT_ON_EXCEPTIONS()
    Stream* strm    = vm->strms[vm->strmCount - 1];
    if( strm->refCount != 0 ) {
        atomic_fetch_sub(&strm->refCount, 1);
        if( strm->refCount == 0 ) {
            fclose(strm->file);
            free(strm);
        }
    }
    --vm->strmCount;
}

uint32_t
vmStreamReadChar(VM* vm, Stream* strm) {
    //ABORT_ON_EXCEPTIONS_V(0)
    char ch = 0;

    if( vmStreamIsEOS(vm, strm) ) {
        return 0;
    }

    fread(&ch, 1, 1, strm->file);
    return ch;
}

bool
vmStreamIsEOS(VM* vm, Stream* strm) {
    //ABORT_ON_EXCEPTIONS_V(true)
    return (bool)feof(strm->file);
}

void
vmStreamWriteChar(VM* vm, Stream* strm, uint32_t ch) {
    //ABORT_ON_EXCEPTIONS()
    fwrite(&ch, 1, 1, strm->file);
}

uint32_t
vmStreamSize(VM* vm, Stream* strm) {
    //ABORT_ON_EXCEPTIONS_V(0)
    int pos = ftell(strm->file);
    fseek(strm->file, 0, SEEK_END);
    uint32_t    len = (uint32_t)ftell(strm->file);
    fseek(strm->file, pos, SEEK_SET);
    return len;
}

uint32_t
vmStreamPos(VM* vm, Stream* strm) {
    //ABORT_ON_EXCEPTIONS_V(0)
    return (uint32_t)ftell(strm->file);
}

void
vmStreamSetPos(VM* vm, Stream* strm, uint32_t pos) {
    //ABORT_ON_EXCEPTIONS()
    fseek(strm->file, (int)pos, SEEK_SET);
}


