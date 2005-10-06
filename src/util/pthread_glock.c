/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/util/pthread_glock.c,v 1.5.2.1 2005/09/21 15:14:47 shadow Exp $");

#if defined(AFS_NT40_ENV) && defined(AFS_PTHREAD_ENV)
#define AFS_GRMUTEX_DECLSPEC __declspec(dllexport)
#endif
#include <afs/pthread_glock.h>
#include <string.h>

/*
 * Implement a pthread based recursive global lock for use in porting
 * old lwp style code to pthreads.
 */

pthread_recursive_mutex_t grmutex;

static int glock_init = 0;
static pthread_once_t glock_init_once = PTHREAD_ONCE_INIT;

static void
glock_init_func(void)
{
    pthread_mutex_init(&grmutex.mut, (const pthread_mutexattr_t *)0);
    grmutex.times_inside = 0;
    grmutex.owner = (pthread_t) 0;
    grmutex.locked = 0;
    glock_init = 1;
}

int
pthread_recursive_mutex_lock(pthread_recursive_mutex_t * mut)
{
    int rc = 0;

    (glock_init || pthread_once(&glock_init_once, glock_init_func));

    if (mut->locked) {
	if (pthread_equal(mut->owner, pthread_self())) {
	    mut->times_inside++;
	    return rc;
	}
    }
    rc = pthread_mutex_lock(&mut->mut);
    if (rc == 0) {
	mut->times_inside = 1;
	mut->owner = pthread_self();
	mut->locked = 1;
    }

    return rc;
}

int
pthread_recursive_mutex_unlock(pthread_recursive_mutex_t * mut)
{
    int rc = 0;

    (glock_init || pthread_once(&glock_init_once, glock_init_func));

    if ((mut->locked) && (pthread_equal(mut->owner, pthread_self()))) {
	mut->times_inside--;
	if (mut->times_inside == 0) {
	    mut->locked = 0;
	    rc = pthread_mutex_unlock(&mut->mut);
	}
    } else {
	/*
	 * Note that you might want to try to differentiate between
	 * the two possible reasons you're here, but since we don't
	 * hold the mutex, it's useless to try.
	 */
	rc = -1;
    }
    return rc;
}
