/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#ifndef DJGPP
#include <windows.h>
#include <winsock2.h>
#endif /* !DJGPP */
#include <string.h>
#include <malloc.h>

#include <osi.h>
#include <rx/rx.h>

#include "afsd.h"

static osi_once_t cm_utilsOnce;

osi_rwlock_t cm_utilsLock;

cm_space_t *cm_spaceListp;

long cm_MapRPCError(long error, cm_req_t *reqp)
{
    if (error == 0) 
        return 0;

    /* If we had to stop retrying, report our saved error code. */
    if (reqp && error == CM_ERROR_TIMEDOUT) {
        if (reqp->accessError)
            return reqp->accessError;
        if (reqp->volumeError)
            return reqp->volumeError;
        if (reqp->rpcError)
            return reqp->rpcError;
        return error;
    }

    if (error < 0) 
        error = CM_ERROR_TIMEDOUT;
    else if (error == 30) 
        error = CM_ERROR_READONLY;
    else if (error == 13) 
        error = CM_ERROR_NOACCESS;
    else if (error == 18) 
        error = CM_ERROR_CROSSDEVLINK;
    else if (error == 17) 
        error = CM_ERROR_EXISTS;
    else if (error == 20) 
        error = CM_ERROR_NOTDIR;
    else if (error == 2) 
        error = CM_ERROR_NOSUCHFILE;
    else if (error == 11        /* EAGAIN, most servers */
             || error == 35)	/* EAGAIN, Digital UNIX */
        error = CM_ERROR_WOULDBLOCK;
    else if (error == VDISKFULL
              || error == 28)   /* ENOSPC */ 
        error = CM_ERROR_SPACE;
    else if (error == VOVERQUOTA
              || error == 49    /* EDQUOT on Solaris */
              || error == 88    /* EDQUOT on AIX */
              || error == 69    /* EDQUOT on Digital UNIX and HPUX */
              || error == 122   /* EDQUOT on Linux */
              || error == 1133) /* EDQUOT on Irix  */
        error = CM_ERROR_QUOTA;
    else if (error == VNOVNODE) 
        error = CM_ERROR_BADFD;
    else if (error == 21)
        return CM_ERROR_ISDIR;
    return error;
}

long cm_MapRPCErrorRmdir(long error, cm_req_t *reqp)
{
    if (error == 0) 
        return 0;

    /* If we had to stop retrying, report our saved error code. */
    if (reqp && error == CM_ERROR_TIMEDOUT) {
        if (reqp->accessError)
            return reqp->accessError;
        if (reqp->volumeError)
            return reqp->volumeError;
        if (reqp->rpcError)
            return reqp->rpcError;
        return error;
    }

    if (error < 0) 
        error = CM_ERROR_TIMEDOUT;
    else if (error == 30) 
        error = CM_ERROR_READONLY;
    else if (error == 20) 
        error = CM_ERROR_NOTDIR;
    else if (error == 13) 
        error = CM_ERROR_NOACCESS;
    else if (error == 2) 
        error = CM_ERROR_NOSUCHFILE;
    else if (error == 17		/* AIX */
              || error == 66		/* SunOS 4, Digital UNIX */
              || error == 93		/* Solaris 2, IRIX */
              || error == 247)	/* HP/UX */
        error = CM_ERROR_NOTEMPTY;
    return error;
}       

long cm_MapVLRPCError(long error, cm_req_t *reqp)
{
	if (error == 0) return 0;

	/* If we had to stop retrying, report our saved error code. */
	if (reqp && error == CM_ERROR_TIMEDOUT) {
		if (reqp->accessError)
			return reqp->accessError;
		if (reqp->volumeError)
			return reqp->volumeError;
		if (reqp->rpcError)
			return reqp->rpcError;
		return error;
	}

	if (error < 0) 
            error = CM_ERROR_TIMEDOUT;
	else if (error == VL_NOENT) 
            error = CM_ERROR_NOSUCHVOLUME;
	return error;
}

cm_space_t *cm_GetSpace(void)
{
	cm_space_t *tsp;

	if (osi_Once(&cm_utilsOnce)) {
		lock_InitializeRWLock(&cm_utilsLock, "cm_utilsLock");
		osi_EndOnce(&cm_utilsOnce);
        }
        
        lock_ObtainWrite(&cm_utilsLock);
	if (tsp = cm_spaceListp) {
		cm_spaceListp = tsp->nextp;
        }
        else tsp = (cm_space_t *) malloc(sizeof(cm_space_t));
	(void) memset(tsp, 0, sizeof(cm_space_t));
        lock_ReleaseWrite(&cm_utilsLock);
        
        return tsp;
}

void cm_FreeSpace(cm_space_t *tsp)
{
        lock_ObtainWrite(&cm_utilsLock);
	tsp->nextp = cm_spaceListp;
	cm_spaceListp = tsp;
        lock_ReleaseWrite(&cm_utilsLock);
}
