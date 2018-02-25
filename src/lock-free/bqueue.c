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
#include <string.h>

#include "lock-free.h"

static inline
FirstLast
FirstLast_incFirst(FirstLast fl) {
    return (FirstLast) { .fl = { .first = fl.fl.first + 1, .last = fl.fl.last } };
}

static inline
FirstLast
FirstLast_incLast(FirstLast fl) {
    return (FirstLast) { .fl = { .first = fl.fl.first, .last = fl.fl.last + 1 } };
}

static inline
uint32_t
FirstLast_remaining(FirstLast fl, uint32_t cap) {
    return (fl.fl.first > fl.fl.last) ? (fl.fl.last - fl.fl.first) : (fl.fl.last + cap - fl.fl.first);
}

static inline
uint32_t
FirstLast_count(FirstLast fl, uint32_t cap) {
    uint32_t    remaining   = FirstLast_remaining(fl, cap);
    return cap - remaining;
}

BoundedQueue*
BoundedQueue_init(BoundedQueue* bq, uint32_t cap) {
    memset(bq, 0, sizeof(BoundedQueue));
    bq->cap             = cap;
    bq->elements        = calloc(cap, sizeof(Element));
    return bq;
}

void
BoundedQueue_release(BoundedQueue* bq) {
    free(bq->elements);
}

bool
BoundedQueue_push(BoundedQueue* bq, void* data) {
    Element*    el  = NULL;
    FirstLast   fl  = (FirstLast)atomic_load_explicit(&bq->fl.u64, memory_order_acquire);
    while(true) {
        el  = &bq->elements[fl.fl.last];
        uint32_t seq  = atomic_load_explicit(&el->data.seq_id, memory_order_acquire);
        int32_t diff  = (int32_t)seq - (int32_t)fl.fl.last;
        if( diff == 0 ) {

        } else {

        }

    }

    return false;
}

void*
BoundedQueue_pop(BoundedQueue* bq) {
    return NULL;
}
