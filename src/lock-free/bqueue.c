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

typedef union {
    struct {
        uint32_t    first;
        uint32_t    last;
    } fl;
    uint64_t    u64;
} FirstLast;

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

typedef struct {
    FirstLast   fl;
    uint32_t    cap;
    void**      elements;
} BoundedQueue;

BoundedQueue*
BoundedQueue_init(BoundedQueue* bq, uint32_t cap) {
    memset(bq, 0, sizeof(BoundedQueue));
    bq->cap             = cap;
    bq->elements        = calloc(cap, sizeof(void*));
    return bq;
}

void
BoundedQueue_release(BoundedQueue* bq) {
    free(bq->elements);
}

bool
BoundedQueue_push(BoundedQueue* bq, void* data) {
    FirstLast   fl          = bq->fl;
    uint32_t    cap         = bq->cap;
    if( FirstLast_remaining(fl, cap) == 0 ) return false;

    atomic_store(&(bq->elements[fl.fl.last % cap]), data);

    while( !atomic_compare_exchange_weak(&bq->fl.u64, &fl.u64, FirstLast_incLast(fl).u64) ) {
        if( FirstLast_remaining(fl, cap) >= cap ) return false;
    }

    return true;
}

void*
BoundedQueue_pop(BoundedQueue* bq) {
    FirstLast   fl      = bq->fl;
    uint32_t    cap     = bq->cap;
    if( FirstLast_count(fl, cap) == 0 ) { return NULL; }
    while( !atomic_compare_exchange_weak(&bq->fl.u64, &fl.u64, FirstLast_incFirst(fl).u64) ) {
        if( FirstLast_count(fl, cap) == 0 ) { return NULL; }
    }

    return atomic_load(&(bq->elements[fl.fl.first % cap]));
}
