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

typedef union {
    struct {
        uint32_t    first;
        uint32_t    last;
    } fl;
    uint64_t    u64;
} FirstLast;

typedef struct {
    FirstLast   fl;
    uint32_t    cap;
    void**      elements;
} BoundedQueue;

BoundedQueue*   BoundedQueue_init(BoundedQueue* bq, uint32_t cap);
void            BoundedQueue_release(BoundedQueue* bq);
bool            BoundedQueue_push(BoundedQueue* bq, void* data);
void*           BoundedQueue_pop(BoundedQueue* bq);
