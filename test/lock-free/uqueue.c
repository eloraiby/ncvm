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

#define MAX_QUEUE_SIZE      1024

void*
producer(void* _bq) {
    Queue*   bq  = _bq;

    size_t  sum = 0;
    for( size_t i = 1; i < 20 * MAX_QUEUE_SIZE; ++i ) {
        Queue_push(bq, (void*)i);
        sum += i;
    }

    fprintf(stderr, "**** producer sum: %u ****\n", sum);
    return (void*)sum;
}

void*
consumer(void* _bq) {
    Queue*   bq  = _bq;

    size_t  sum = 0;
    for( size_t i = 1; i < 20 * MAX_QUEUE_SIZE; ++i ) {

        bool    succeeded = false;
        while( succeeded == false ) {
            void* v = NULL;
            if( (v = Queue_pop(bq)) != NULL ) {
                //fprintf(stderr, "poped: %u\n", v);
                sum += v;
                succeeded   = true;
            } else {
                fprintf(stderr, "-- consumer yielded (%u) --\n", i);
                pthread_yield();
            }
        }
    }

    fprintf(stderr, "**** consumer sum: %u ****\n", sum);
    return (void*)sum;
}

int
main(int argc, char* argv[]) {

    pthread_t   prod, cons;
    Queue*      bq  = Queue_new();
    pthread_create(&prod, NULL, producer, bq);
    pthread_create(&cons, NULL, consumer, bq);


    size_t  sprod   = 0;
    size_t  scons   = 0;
    pthread_join(prod, (void**)&sprod);
    pthread_join(cons, (void**)&scons);

    assert( sprod == scons );
    Queue_release(bq);
    return 0;
}
