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
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <memory.h>

typedef struct {
    uint32_t    count;
    uint32_t    cap;
    void**      elements;
} BoundedStack;

BoundedStack*
BoundedStack_init(BoundedStack* bs, uint32_t cap) {
    memset(bs, 0, sizeof(BoundedStack));
    bs->cap = cap;
    bs->elements    = calloc(cap, sizeof(void*));
    return bs;
}

void
BoundedStack_release(BoundedStack* bs) {
    free(bs->elements);
    bs->cap     = 0;
    bs->count   = 0;
}

bool
BoundedStack_push(BoundedStack* bs, void* e) {
    uint32_t    count   = bs->count;

    if( (count + 1) >= bs->cap ) return false;
    while( !atomic_compare_exchange_weak(&bs->count, &count, count + 1) ) {
        if( (count + 1) >= bs->cap ) return false;
    }
    bs->elements[count] = e;
    return true;
}

void*
BoundedStack_pop(BoundedStack* bs) {
    uint32_t    count   = bs->count;

    if( count == 0 ) return false;
    while( !atomic_compare_exchange_weak(&bs->count, &count, count - 1) ) {
        if( count == 0 ) return false;
    }
    return bs->elements[count - 1];
}
