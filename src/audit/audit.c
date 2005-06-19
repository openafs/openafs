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

#include <fcntl.h>
#include <stdarg.h>
#ifdef AFS_AIX32_ENV
#include <sys/audit.h>
#endif /* AFS_AIX32_ENV */
#include <errno.h>

#include "afs/afsint.h"
#include <rx/rx.h>
#include <rx/rxkad.h>
#include "audit.h"
#include "lock.h"
#ifdef AFS_AIX32_ENV
#include <sys/audit.h>
#endif
#include <afs/afsutil.h>

char *bufferPtr;
int bufferLen;
int osi_audit_all = (-1);	/* Not determined yet */
int osi_echo_trail = (-1);

#ifdef AFS_AIX_ENV /** all these functions are only defined for AIX */

#ifndef	AFS_OSF_ENV
/*
 * These variadic functions work under AIX, and not all systems (osf1)
 */
/* ************************************************************************** */
/* AIX requires a buffer filled with values to record with each audit event.
 * aixmakebuf creates that buffer from the variable list of values we are given.
 * ************************************************************************** */
static void
aixmakebuf(char *audEvent, char *vaList)
{
    int code;
    int vaEntry;
    int vaInt;
    afs_int32 vaLong;
    char *vaStr;
    char *vaLst;
    char hname[20];
    struct AFSFid *vaFid;

    vaEntry = va_arg(vaList, int);
    while (vaEntry != AUD_END) {
	switch (vaEntry) {
	case AUD_STR:		/* String */
	    vaStr = (char *)va_arg(vaList, int);
	    if (vaStr) {
		strcpy(bufferPtr, vaStr);
		bufferPtr += strlen(vaStr) + 1;
	    } else {
		strcpy(bufferPtr, "");
		bufferPtr++;
	    }
	    break;
	case AUD_INT:		/* Integer */
	    vaInt = va_arg(vaList, int);
	    *(int *)bufferPtr = vaInt;
	    bufferPtr += sizeof(vaInt);
	    break;
	case AUD_DATE:		/* Date    */
	case AUD_HOST:		/* Host ID */
	case AUD_LONG:		/* long    */
	    vaLong = va_arg(vaList, afs_int32);
	    *(afs_int32 *) bufferPtr = vaLong;
	    bufferPtr += sizeof(vaLong);
	    break;
	case AUD_LST:		/* Ptr to another list */
	    vaLst = (char *)va_arg(vaList, int);
	    aixmakebuf(audEvent, vaLst);
	    break;
	case AUD_FID:		/* AFSFid - contains 3 entries */
	    vaFid = (struct AFSFid *)va_arg(vaList, int);
	    if (vaFid) {
		memcpy(bufferPtr, vaFid, sizeof(struct AFSFid));
	    } else {
		memset(bufferPtr, 0, sizeof(struct AFSFid));
	    }
	    bufferPtr += sizeof(struct AFSFid);
	    break;

	    /* Whole array of fids-- don't know how to handle variable length audit
	     * data with AIX audit package, so for now we just store the first fid.
	     * Better one than none. */
	case AUD_FIDS:
	    {
		struct AFSCBFids *Fids;

		Fids = (struct AFSCBFids *)va_arg(vaList, int);
		if (Fids && Fids->AFSCBFids_len) {
		    *((u_int *) bufferPtr) = Fids->AFSCBFids_len;
		    bufferPtr += sizeof(u_int);
		    memcpy(bufferPtr, Fids->AFSCBFids_val,
			   sizeof(struct AFSFid));
		} else {
		    struct AFSFid dummy;
		    *((u_int *) bufferPtr) = 0;
		    bufferPtr += sizeof(u_int);
		    memset(bufferPtr, 0, sizeof(struct AFSFid));
		}
		bufferPtr += sizeof(struct AFSFid);
		break;
	    }
	default:
#ifdef AFS_AIX32_ENV
	    code =
		auditlog("AFS_Aud_EINVAL", (-1), audEvent,
			 (strlen(audEvent) + 1));
#endif
	    return;
	    break;
	}			/* end switch */

	vaEntry = va_arg(vaList, int);
    }				/* end while */
}

static void
printbuf(char *audEvent, afs_int32 errCode, char *vaList)
{
    int vaEntry;
    int vaInt;
    afs_int32 vaLong;
    char *vaStr;
    char *vaLst;
    char hname[20];
    struct AFSFid *vaFid;
    struct AFSCBFids *vaFids;

    if (osi_echo_trail < 0)
	osi_audit_check();
    if (!osi_echo_trail)
	return;

    if (strcmp(audEvent, "VALST") != 0)
	printf("%s %d ", audEvent, errCode);

    vaEntry = va_arg(vaList, int);
    while (vaEntry != AUD_END) {
	switch (vaEntry) {
	case AUD_STR:		/* String */
	    vaStr = (char *)va_arg(vaList, int);
	    if (vaStr)
		printf("%s ", vaStr);
	    else
		printf("<null>", vaStr);
	    break;
	case AUD_INT:		/* Integer */
	    vaInt = va_arg(vaList, int);
	    printf("%d ", vaInt);
	    break;
	case AUD_DATE:		/* Date    */
	case AUD_HOST:		/* Host ID */
	    vaLong = va_arg(vaList, afs_int32);
	    printf("%u ", vaLong);
	    break;
	case AUD_LONG:		/* afs_int32    */
	    vaLong = va_arg(vaList, afs_int32);
	    printf("%d ", vaLong);
	    break;
	case AUD_LST:		/* Ptr to another list */
	    vaLst = (char *)va_arg(vaList, int);
	    printbuf("VALST", 0, vaLst);
	    break;
	case AUD_FID:		/* AFSFid - contains 3 entries */
	    vaFid = (struct AFSFid *)va_arg(vaList, int);
	    if (vaFid)
		printf("%u:%u:%u ", vaFid->Volume, vaFid->Vnode,
		       vaFid->Unique);
	    else
		printf("%u:%u:%u ", 0, 0, 0);
	    break;
	case AUD_FIDS:		/* array of Fids */
	    vaFids = (struct AFSCBFids *)va_arg(vaList, int);
	    vaFid = NULL;

	    if (vaFids)
		vaFid = vaFids->AFSCBFids_val;
	    if (vaFid)
		printf("%u %u:%u:%u ", vaFids->AFSCBFids_len, vaFid->Volume,
		       vaFid->Vnode, vaFid->Unique);
	    else
		printf("0 0:0:0 ");
	    break;
	default:
	    printf("--badval-- ");
	    break;
	}			/* end switch */
	vaEntry = va_arg(vaList, int);
    }				/* end while */

    if (strcmp(audEvent, "VALST") != 0)
	printf("\n");
}
#else
static void
aixmakebuf(audEvent, vaList)
     char *audEvent;
     va_list vaList;
{
    return;
}

static void
printbuf(char *audEvent, long errCode, va_list vaList)
{
    return;
}

#endif


/* ************************************************************************** */
/* The routine that acually does the audit call.
 * ************************************************************************** */
int
osi_audit(char *audEvent,	/* Event name (15 chars or less) */
	  afs_int32 errCode,	/* The error code */
	  ...)
{
#ifdef AFS_AIX32_ENV
    afs_int32 code;
    afs_int32 err;
    int result;

    va_list vaList;
    static struct Lock audbuflock = { 0, 0, 0, 0,
#ifdef AFS_PTHREAD_ENV
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_COND_INITIALIZER,
	PTHREAD_COND_INITIALIZER
#endif /* AFS_PTHREAD_ENV */
    };
    static char BUFFER[32768];

    if (osi_audit_all < 0)
	osi_audit_check();
    if (!osi_audit_all)
	return;

    switch (errCode) {
    case 0:
	result = AUDIT_OK;
	break;
    case KANOAUTH:		/* kautils.h   */
    case RXKADNOAUTH:		/* rxkad.h     */
	result = AUDIT_FAIL_AUTH;
	break;
    case EPERM:		/* errno.h     */
    case EACCES:		/* errno.h     */
    case PRPERM:		/* pterror.h   */
	result = AUDIT_FAIL_ACCESS;
	break;
    case VL_PERM:		/* vlserver.h  */
    case BUDB_NOTPERMITTED:	/* budb_errs.h */
/*  case KRB_RD_AP_UNAUTHOR : */
    case BZACCESS:		/* bnode.h     */
    case VOLSERBAD_ACCESS:	/* volser.h    */
	result = AUDIT_FAIL_PRIV;
	break;
    default:
	result = AUDIT_FAIL;
	break;
    }

    ObtainWriteLock(&audbuflock);
    bufferPtr = BUFFER;

    /* Put the error code into the buffer list */
    *(int *)bufferPtr = errCode;
    bufferPtr += sizeof(errCode);

    va_start(vaList, errCode);
    aixmakebuf(audEvent, vaList);

    va_start(vaList, errCode);
    printbuf(audEvent, errCode, vaList);

    bufferLen = (int)((afs_int32) bufferPtr - (afs_int32) & BUFFER[0]);
    code = auditlog(audEvent, result, BUFFER, bufferLen);
#ifdef notdef
    if (code) {
	err = errno;
	code = auditlog("AFS_Aud_Fail", result, &err, sizeof(err));
	if (code)
	    printf("Error while writing audit entry: %d.\n", errno);
    }
#endif /* notdef */
    ReleaseWriteLock(&audbuflock);
#endif
}

/* ************************************************************************** */
/* Given a RPC call structure, this routine extracts the name and host id from the 
 * call and includes it within the audit information.
 * ************************************************************************** */
int
osi_auditU(struct rx_call *call, char *audEvent, int errCode, ...)
{
    struct rx_connection *conn;
    struct rx_peer *peer;
    afs_int32 secClass;
    afs_int32 code;
    char afsName[MAXKTCNAMELEN];
    afs_int32 hostId;
    va_list vaList;


    if (osi_audit_all < 0)
	osi_audit_check();
    if (!osi_audit_all)
	return;

    strcpy(afsName, "--Unknown--");
    hostId = 0;

    if (call) {
	conn = rx_ConnectionOf(call);	/* call -> conn) */
	if (conn) {
	    secClass = rx_SecurityClassOf(conn);	/* conn -> securityIndex */
	    if (secClass == 0) {	/* unauthenticated */
		osi_audit("AFS_Aud_Unauth", (-1), AUD_STR, audEvent, AUD_END);
		strcpy(afsName, "--UnAuth--");
	    } else if (secClass == 2) {	/* authenticated */
		code =
		    rxkad_GetServerInfo(conn, NULL, NULL, afsName, NULL, NULL,
					NULL);
		if (code) {
		    osi_audit("AFS_Aud_NoAFSId", (-1), AUD_STR, audEvent,
			      AUD_END);
		    strcpy(afsName, "--NoName--");
		}
	    } else {		/* Unauthenticated & unknown */

		osi_audit("AFS_Aud_UnknSec", (-1), AUD_STR, audEvent,
			  AUD_END);
	    }

	    peer = rx_PeerOf(conn);	/* conn -> peer */
	    if (peer)
		hostId = rx_HostOf(peer);	/* peer -> host */
	    else
		osi_audit("AFS_Aud_NoHost", (-1), AUD_STR, audEvent, AUD_END);
	} else {		/* null conn */

	    osi_audit("AFS_Aud_NoConn", (-1), AUD_STR, audEvent, AUD_END);
	}
    } else {			/* null call */

	osi_audit("AFS_Aud_NoCall", (-1), AUD_STR, audEvent, AUD_END);
    }

    va_start(vaList, errCode);
    osi_audit(audEvent, errCode, AUD_STR, afsName, AUD_HOST, hostId, AUD_LST,
	      vaList, AUD_END);
}

/* ************************************************************************** */
/* Determines whether auditing is on or off by looking at the Audit file.
 * If the string AFS_AUDIT_AllEvents is defined in the file, then auditing will be 
 * enabled.
 * ************************************************************************** */

int
osi_audit_check()
{
    FILE *fds;
    int onoff;
    char event[257];

    osi_audit_all = 1;		/* say we made check (>= 0) */
    /* and assume audit all events (for now) */
    onoff = 0;			/* assume we will turn auditing off */
    osi_echo_trail = 0;		/* assume no echoing   */

    fds = fopen(AFSDIR_SERVER_AUDIT_FILEPATH, "r");
    if (fds) {
	while (fscanf(fds, "%256s", event) > 0) {
	    if (strcmp(event, "AFS_AUDIT_AllEvents") == 0)
		onoff = 1;

	    if (strcmp(event, "Echo_Trail") == 0)
		osi_echo_trail = 1;
	}
	fclose(fds);
    }

    /* Audit this event all of the time */
    if (onoff)
	osi_audit("AFS_Aud_On", 0, AUD_END);
    else
	osi_audit("AFS_Aud_Off", 0, AUD_END);

    /* Now set whether we audit all events from here on out */
    osi_audit_all = onoff;
}


#else /* ! AFS_AIX_ENV */

int
osi_audit(char *audEvent, afs_int32 errCode, ...)
{
    return 0;
}

int
osi_auditU(struct rx_call *call, char *audEvent, int errCode, ...)
{
    return 0;
}

#endif
