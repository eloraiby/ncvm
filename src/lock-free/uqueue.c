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


typedef struct Node Node;

struct Node {
	void*   data;
	Node*   next;
};

typedef struct {
	Node*   first;
	Node*   last;
} Queue;

Queue*
Queue_new() {
	Queue*  q   = calloc(1, sizeof(Queue));
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
	free(q);
}

void
Qeue_push(Queue* q, void *data) {
	Node*   n       = calloc(1, sizeof(Node));
	Node*   last    = q->last;
	while( !atomic_compare_exchange_weak(&q->last, &last, n) ) {
		last    = q->last;
	}
	atomic_store(&last->next , n);
	last->data  = data;
}

void*
Queue_pop(Queue* q) {
	while( 1 ) {
		Node*   first   = q->first;
		Node*   next    = first->next;

		if( !first->data ) continue;

		while( !atomic_compare_exchange_strong(&q->first, &first, next) ) {
			first   = q->first;
			next    = atomic_load(&first->next);
		}

		return first->data;
	}
}

