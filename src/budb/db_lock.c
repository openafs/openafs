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
    ("$Header$");

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/time.h>
#endif
#include <afs/afsutil.h>
#include <ubik.h>
#include <afs/bubasics.h>
#include "budb_errs.h"
#include "database.h"
#include "error_macros.h"
#include "afs/audit.h"

#define	DBH_POS(ptr)		( (char *) (ptr) - (char *) &db.h )

afs_int32 FreeAllLocks(), FreeLock(), GetInstanceId(), GetLock();

afs_int32
SBUDB_FreeAllLocks(call, instanceId)
     struct rx_call *call;
     afs_uint32 instanceId;
{
    afs_int32 code;

    code = FreeAllLocks(call, instanceId);
    osi_auditU(call, BUDB_FrALckEvent, code, AUD_END);
    return code;
}

afs_int32
FreeAllLocks(call, instanceId)
     struct rx_call *call;
     afs_uint32 instanceId;
{
    db_lockP startPtr, endPtr;
    struct ubik_trans *ut;
    afs_int32 code;

    if (callPermitted(call) == 0)
	return (BUDB_NOTPERMITTED);

    code = InitRPC(&ut, LOCKWRITE, 1);
    if (code)
	return (code);

    startPtr = &db.h.textLocks[0];
    endPtr = &db.h.textLocks[TB_NUM - 1];
    while (startPtr <= endPtr) {
	if ((ntohl(startPtr->lockState) == 1)
	    && (ntohl(startPtr->instanceId) == instanceId)
	    ) {
	    /* release the lock */
	    startPtr->lockState = 0;	/* unlock it */
	    startPtr->lockTime = 0;
	    startPtr->expires = 0;
	    startPtr->instanceId = 0;
	    dbwrite(ut, DBH_POS(startPtr), (char *)startPtr,
		    sizeof(db_lockT));
	}
	startPtr++;
    }
    code = ubik_EndTrans(ut);
    return (code);
}

afs_int32
SBUDB_FreeLock(call, lockHandle)
     struct rx_call *call;
     afs_uint32 lockHandle;
{
    afs_int32 code;

    code = FreeLock(call, lockHandle);
    osi_auditU(call, BUDB_FreLckEvent, code, AUD_END);
    return code;
}

afs_int32
FreeLock(call, lockHandle)
     struct rx_call *call;
     afs_uint32 lockHandle;
{
    db_lockP lockPtr = 0;
    struct ubik_trans *ut;
    afs_int32 code;

    if (callPermitted(call) == 0)
	return (BUDB_NOTPERMITTED);

    code = InitRPC(&ut, LOCKWRITE, 1);
    if (code)
	return (code);

    if (checkLockHandle(ut, lockHandle) == 0)
	ABORT(BUDB_BADARGUMENT);

    lockPtr = &db.h.textLocks[lockHandle - 1];

    lockPtr->lockState = 0;	/* unlock it */
    lockPtr->lockTime = 0;
    lockPtr->expires = 0;
    lockPtr->instanceId = 0;
    dbwrite(ut, DBH_POS(lockPtr), (char *)lockPtr, sizeof(db_lockT));

    code = ubik_EndTrans(ut);
    return (code);

  abort_exit:
    ubik_AbortTrans(ut);
    return (code);
}

afs_int32
SBUDB_GetInstanceId(call, instanceId)
     struct rx_call *call;
     afs_uint32 *instanceId;
{
    afs_int32 code;

    code = GetInstanceId(call, instanceId);
    osi_auditU(call, BUDB_GetIIdEvent, code, AUD_END);
    return code;
}

afs_int32
GetInstanceId(call, instanceId)
     struct rx_call *call;
     afs_uint32 *instanceId;
{
    struct ubik_trans *ut;
    afs_int32 code;
    afs_int32 instanceValue;

    LogDebug(4, "GetInstanceId:\n");

    /* *** Allow anyone to get the instance id ***
     * if ( callPermitted(call) == 0 )
     *    return(BUDB_NOTPERMITTED);
     */

    code = InitRPC(&ut, LOCKWRITE, 1);
    if (code)
	return (code);

    instanceValue = ntohl(db.h.lastInstanceId) + 1;

    set_header_word(ut, lastInstanceId, htonl(instanceValue));

    code = ubik_EndTrans(ut);
    return (code);
}


afs_int32
SBUDB_GetLock(call, instanceId, lockName, expiration, lockHandle)
     struct rx_call *call;
     afs_uint32 instanceId;
     afs_int32 lockName;
     afs_int32 expiration;
     afs_uint32 *lockHandle;
{
    afs_int32 code;

    code = GetLock(call, instanceId, lockName, expiration, lockHandle);
    osi_auditU(call, BUDB_GetLckEvent, code, AUD_END);
    return code;
}

afs_int32
GetLock(call, instanceId, lockName, expiration, lockHandle)
     struct rx_call *call;
     afs_uint32 instanceId;
     afs_int32 lockName;
     afs_int32 expiration;
     afs_uint32 *lockHandle;
{
    struct timeval tv;
    db_lockP lockPtr;
    struct ubik_trans *ut;

    afs_int32 code;

    if (callPermitted(call) == 0)
	return (BUDB_NOTPERMITTED);

    if ((lockName < 0) || (lockName >= TB_NUM))
	return (BUDB_BADARGUMENT);

    /* get the current time */
    gettimeofday(&tv, 0);

    code = InitRPC(&ut, LOCKWRITE, 1);
    if (code)
	return (code);

    lockPtr = &db.h.textLocks[lockName];

    if ((ntohl(lockPtr->lockState) != 0)	/* lock set */
	&&(ntohl(lockPtr->expires) > tv.tv_sec)	/* not expired */
	) {
	if (ntohl(lockPtr->instanceId) == instanceId)
	    code = BUDB_SELFLOCKED;
	else
	    code = BUDB_LOCKED;
	goto abort_exit;
    }

    lockPtr->lockState = htonl(1);	/* lock it */
    lockPtr->lockTime = htonl(tv.tv_sec);	/* when locked */
    lockPtr->expires = htonl(tv.tv_sec + expiration);
    lockPtr->instanceId = htonl(instanceId);
    code = dbwrite(ut, DBH_POS(lockPtr), (char *)lockPtr, sizeof(db_lockT));
    if (code)
	ABORT(code);

    *lockHandle = (afs_uint32) (lockName + 1);
    code = ubik_EndTrans(ut);
    return (code);

  abort_exit:
    ubik_AbortTrans(ut);
    return (code);
}


/* checkLockHandle
 * exit:
 *	0 - if invalid handle
 *	1 - if handle is valid
 */

checkLockHandle(ut, lockHandle)
     struct ubik_trans *ut;
     afs_uint32 lockHandle;
{
    return (((lockHandle > 0) && (lockHandle <= TB_NUM)) ? 1 : 0);
}
