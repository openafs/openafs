/*
 * Copyright (C)  1998  Transarc Corporation.  All rights reserved.
 *
 *
 * Test thread specific data functionality.
 *
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>

#define NTH 10

pthread_key_t key;

void destruct(void *val) {
}

void * threadFunc(void *arg) {

    pthread_setspecific(key, arg);
    assert(pthread_getspecific(key) == arg);
    return (void *) 0;
}


int main(int argc, char **argv) {

    pthread_t tid[NTH];
    int i,j;

    i = pthread_key_create(&key,destruct);
    assert(i==0);

    for(j=0;j<NTH;j++) {
	i = pthread_create(&tid[j], (const pthread_attr_t *) 0,
	    threadFunc, (void *) j);
	assert(i==0);
    }
    for(j=0;j<NTH;j++) {
	i = pthread_join(tid[j], (void **) 0);
	assert(i==0);
    }
    return 0;
}
