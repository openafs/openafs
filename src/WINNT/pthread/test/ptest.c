/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Test basic pthread functionality
 *
 * This is a very simple test program that exercises the pthread library
 * for NT.  The output of this program for a successful run is no
 * text at all.  assert's are used to verify the functionality.
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
#define SLEEP_TIME 2*1000 /* 2 seconds */
#define COND_WAIT_TIME 3

static pthread_once_t once_c = PTHREAD_ONCE_INIT;

static int should_be_one;
static int should_be_NTH;
static int should_be_NTH_minus_1;

static pthread_mutex_t g_mutex;
static pthread_mutex_t try_mutex;
static pthread_cond_t g_cond;
static pthread_mutex_t cond_mutex;
static int cond_flag;

static pthread_once_t error_once;
static pthread_mutex_t error_mutex;
static pthread_cond_t error_cond;

void once_me(void) {
    should_be_one++;
    assert(pthread_mutex_init(&g_mutex, (const pthread_mutexattr_t *) 0)==0);
    assert(pthread_mutex_init(&try_mutex, (const pthread_mutexattr_t *) 0)==0);
    assert(pthread_cond_init(&g_cond, (const pthread_condattr_t *) 0)==0);
    assert(pthread_mutex_init(&cond_mutex, (const pthread_mutexattr_t *) 0)==0);
}

void * threadFunc(void *arg) {
    pthread_t *me = (pthread_t *) arg;

    assert(pthread_equal(*me, pthread_self()));
    assert(pthread_once(&once_c, once_me)==0);
    assert(pthread_mutex_lock(&g_mutex)==0);
    should_be_NTH++;
    assert(pthread_mutex_unlock(&g_mutex)==0);
    /*
     * Test trylock - assumes all other threads will pass the trylock
     * call in 0 seconds.
     */
    if(pthread_mutex_trylock(&try_mutex)==0) {
	Sleep(SLEEP_TIME);
	assert(pthread_mutex_unlock(&try_mutex)==0);
    } else {
	should_be_NTH_minus_1++;
    }
    pthread_exit((void *) 0);
    return (void *) 0; /* eliminate a warning */
}


void * condWaitFunc(void *arg) {
    assert(pthread_mutex_lock(&cond_mutex)==0);
    while(cond_flag == 0) {
	assert(pthread_cond_wait(&g_cond, &cond_mutex)==0);
    }
    assert(pthread_mutex_unlock(&cond_mutex)==0);
    return (void *) 0;
}

void * condTimeWaitFunc(void *arg) {
    timespec_t t = {(time((time_t *) 0) + COND_WAIT_TIME),0};

    assert(pthread_mutex_lock(&cond_mutex)==0);
    assert(pthread_cond_timedwait(&g_cond, &cond_mutex, &t)==ETIME);
    assert(pthread_mutex_unlock(&cond_mutex)==0);
    return (void *) 0;
}

void * condSignalFunc(void *arg) {
    Sleep(SLEEP_TIME);
    cond_flag = 1;
    assert(pthread_cond_signal(&g_cond)==0);
    return (void *) 0;
}

void * condBroadcastFunc(void *arg) {
    Sleep(SLEEP_TIME);
    cond_flag = 1;
    assert(pthread_cond_broadcast(&g_cond)==0);
    return (void *) 0;
}

int main(int argc, char **argv) {

    pthread_t tid[NTH];
    int i,j;

    /*
     * Test "successful" function calls
     */

    /*
     * Test mutex interface
     */
    for(j=0;j<NTH;j++) {
	i = pthread_create(&tid[j], (const pthread_attr_t *) 0,
	    threadFunc, (void *) &tid[j]);
	assert(i==0);
    }
    for(j=0;j<NTH;j++) {
	i = pthread_join(tid[j], (void **) 0);
	assert(i==0);
    }
    assert(pthread_mutex_destroy(&g_mutex)==0);
    assert(should_be_one == 1);
    assert(should_be_NTH == NTH);
    assert(should_be_NTH_minus_1 == (NTH-1));

    /*
     * Test condition interface
     */

    assert(pthread_create(&tid[0], (const pthread_attr_t *) 0, condWaitFunc,
	   (void *) 0)==0);
    assert(pthread_create(&tid[1], (const pthread_attr_t *) 0, condSignalFunc,
	   (void *) 0)==0);
    assert(pthread_join(tid[0], (void **) 0)==0);
    assert(pthread_join(tid[1], (void **) 0)==0);

    cond_flag = 0;
    for(j=0;j<(NTH-1);j++) {
	i = pthread_create(&tid[j], (const pthread_attr_t *) 0,
	    condWaitFunc, (void *) 0);
	assert(i==0);
    }
    assert(pthread_create(&tid[NTH-1], (const pthread_attr_t *) 0, 
	   condBroadcastFunc, (void *) 0)==0);
    for(j=0;j<NTH;j++) {
	i = pthread_join(tid[j], (void **) 0);
	assert(i==0);
    }

    cond_flag = 0;
    i = time((time_t *) 0);
    assert(pthread_create(&tid[0], (const pthread_attr_t *) 0, condTimeWaitFunc,
	   (void *) 0)==0);
    assert(pthread_join(tid[0], (void **) 0)==0);
    assert((time((time_t *) 0) - i) >= (COND_WAIT_TIME - 1));

    assert(pthread_cond_destroy(&g_cond)==0);
    assert(pthread_mutex_destroy(&try_mutex)==0);
    assert(pthread_mutex_destroy(&cond_mutex)==0);

    /*
     * Test explicit error cases
     */

    assert(pthread_once((pthread_once_t *) 0, once_me)!=0);
    assert(pthread_once(&error_once, 0)!=0);
    assert(pthread_mutex_init((pthread_mutex_t *) 0,
	   (const pthread_mutexattr_t *) 0 )!=0);
    assert(pthread_mutex_init(&error_mutex,
	   (const pthread_mutexattr_t *) 4 )!=0);
    assert(pthread_mutex_trylock((pthread_mutex_t *)0)!=0);
    assert(pthread_mutex_lock((pthread_mutex_t *)0)!=0);
    assert(pthread_mutex_unlock((pthread_mutex_t *)0)!=0);
    assert(pthread_mutex_destroy((pthread_mutex_t *)0)!=0);
    assert(pthread_create((pthread_t *) 0, (const pthread_attr_t *) 0,
	   threadFunc, (void *) 0)!=0);
    assert(pthread_cond_init((pthread_cond_t *) 0,
	   (const pthread_condattr_t *) 0)!=0);
    assert(pthread_cond_init(&error_cond, (const pthread_condattr_t *) 4)!=0);
    assert(pthread_cond_wait((pthread_cond_t *) 0, &error_mutex)!=0);
    assert(pthread_cond_wait(&error_cond, (pthread_mutex_t *) 0)!=0);
    assert(pthread_cond_signal((pthread_cond_t *) 0)!=0);
    assert(pthread_cond_broadcast((pthread_cond_t *) 0)!=0);
    assert(pthread_cond_destroy((pthread_cond_t *) 0)!=0);
    return 0;
}
