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
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "lock-free.h"

Queue*
Queue_init(Queue* q) {
    memset(q, 0, sizeof(Queue));
    q->first    = q->last   = calloc(1, sizeof(Node));
    return q;
}

void
Queue_release(Queue* q) {
	Node*   tmp     = NULL;
	Node*   curr    = q->first;
	while( curr ) {
		tmp     = curr->next;
		free(curr);
		curr    = tmp;
	}
}

void
Queue_push(Queue* q, void *data) {
	Node*   n       = calloc(1, sizeof(Node));
    Node*   last    = NULL;
    do {
        last    = atomic_load_explicit(&q->last, memory_order_acquire);
    } while( !atomic_compare_exchange_weak(&q->last, &last, n) );
    atomic_store_explicit(&last->next, n, memory_order_release);
    atomic_store_explicit(&last->data, data, memory_order_release);
}

void*
Queue_pop(Queue* q) {
	while( 1 ) {
        Node*   first   = NULL;
        Node*   next    = NULL;
        void*   data    = NULL;
        bool    set     = false;

        do {
            first   = atomic_load_explicit(&q->first, memory_order_acquire);
            next    = atomic_load_explicit(&first->next, memory_order_acquire);
            if( next == NULL ) { return NULL; } // queue is empty, nothing to do, bail!
        } while( !atomic_compare_exchange_strong(&q->first, &first, next) );

        // wait for the data to come in (this will block if the push was preempted but popping from
        // other threads can continue regardless)
        do {
            data    = atomic_load_explicit(&first->data, memory_order_acquire);
        } while( data == NULL );

        free(first);

        return data;
	}
}

