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

#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include "../../src/lock-free/lock-free.h"

#define MAX_QUEUE_SIZE      64
#define SWITCH 1
void*
producer0(void* _bq) {
    BoundedQueue*   bq  = _bq;

    size_t  sum = 0;
    for( size_t i = 1; i < 1 * MAX_QUEUE_SIZE; ++i ) {
        while(BoundedQueue_push(bq, (void*)i) == false) {
            fprintf(stderr, "-- producer0 yielded (%u) --\n", i);
            pthread_yield();
        }
        sum += i;
    }

    fprintf(stderr, "**** producer0 sum: %u ****\n", sum);
    return (void*)sum;
}

void*
producer1(void* _bq) {
    BoundedQueue*   bq  = _bq;

    size_t  sum = 0;
    for( size_t i = 1 * MAX_QUEUE_SIZE; i < 2 * MAX_QUEUE_SIZE; ++i ) {
        while( BoundedQueue_push(bq, (void*)i) == false) {
            fprintf(stderr, "-- producer1 yielded (%u) --\n", i);
            pthread_yield();
        }
        sum += i;
    }

    fprintf(stderr, "**** producer1 sum: %u ****\n", sum);
    return (void*)sum;
}

void*
consumer0(void* _bq) {
    BoundedQueue*   bq  = _bq;

    size_t  sum = 0;
    for( size_t i = 1; i < 1 * MAX_QUEUE_SIZE; ++i ) {

        bool    succeeded = false;
        while( succeeded == false ) {
            void* v = NULL;
            if( (v = BoundedQueue_pop(bq)) != NULL ) {
                sum += (size_t)v;
                succeeded   = true;
            } else {
                fprintf(stderr, "-- consumer0 yielded (%u) --\n", i);
                pthread_yield();
            }
        }
    }

    fprintf(stderr, "**** consumer0 sum: %u ****\n", sum);
    return (void*)sum;
}

void*
consumer1(void* _bq) {
   BoundedQueue*   bq  = _bq;

    size_t  sum = 0;
    for( size_t i = 1 * MAX_QUEUE_SIZE; i < 2 * MAX_QUEUE_SIZE; ++i ) {

        bool    succeeded = false;
        while( succeeded == false ) {
            void* v = NULL;
            if( (v = BoundedQueue_pop(bq)) != NULL ) {
                //fprintf(stderr, "poped: %u\n", v);
                sum += (size_t)v;
                succeeded   = true;
            } else {
                fprintf(stderr, "-- consumer1 yielded (%u) --\n", i);
                pthread_yield();
            }
        }
    }

    fprintf(stderr, "**** consumer1 sum: %u ****\n", sum);
    return (void*)sum;
}

int
main(int argc, char* argv[]) {

    pthread_t   prod0, prod1, cons0, cons1;
    BoundedQueue    q;
    BoundedQueue_init(&q, MAX_QUEUE_SIZE);
#ifdef SWITCH
    pthread_create(&prod0, NULL, producer0, &q);
    pthread_create(&prod1, NULL, producer1, &q);
    pthread_create(&cons0, NULL, consumer0, &q);
    pthread_create(&cons1, NULL, consumer1, &q);

    size_t  sprod0  = 0;
    size_t  sprod1  = 0;
    size_t  scons0  = 0;
    size_t  scons1  = 0;
    pthread_join(prod0, (void**)&sprod0);
    pthread_join(prod1, (void**)&sprod1);
    pthread_join(cons0, (void**)&scons0);
    pthread_join(cons1, (void**)&scons1);
#else
    size_t  sprod0  = 0;
    size_t  sprod1  = 0;
    size_t  scons0  = 0;
    size_t  scons1  = 0;

    sprod0 = (size_t)producer0(&q);
    scons0 = (size_t)consumer0(&q);
    sprod1 = (size_t)producer1(&q);
    scons1 = (size_t)consumer1(&q);
#endif
    assert( (sprod0 + sprod1) == (scons0 + scons1) );

    if( (sprod0 + sprod1) != (scons0 + scons1) ) {
        fprintf(stderr, "FAIL!!!\n");
    }

    BoundedQueue_release(&q);
    return 0;
}
