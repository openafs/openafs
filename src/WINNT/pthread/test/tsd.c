/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
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
