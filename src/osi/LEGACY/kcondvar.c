/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_mutex.h>
#include <osi/osi_condvar.h>
#include <afs/lock.h>

int
osi_condvar_WaitSig(osi_condvar_t * cv, osi_mutex_t * lock)
{
    int code;
    int haveGlock = ISAFS_GLOCK();
    if (!haveGlock) {
	AFS_GLOCK();
    }
    osi_mutex_Unlock(lock);
    code = afs_osi_SleepSig(cv);
    if (!haveGlock) {
	AFS_GUNLOCK();
    }
    osi_mutex_Lock(lock);
    return code;
}
