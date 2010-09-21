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


#ifdef KERNEL
#if defined(UKERNEL)
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "rx/xdr.h"
#else /* defined(UKERNEL) */
#if defined(AFS_OSF_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
#include "afs/sysincludes.h"
#include "afsincludes.h"
#else
#include "h/types.h"
#include "rpc/types.h"
#include "rx/xdr.h"
#endif
#if !defined(AFS_ALPHA_ENV)
#ifndef	XDR_GETINT32
#define	XDR_GETINT32	XDR_GETLONG
#endif
#ifndef	XDR_PUTINT32
#define	XDR_PUTINT32	XDR_PUTLONG
#endif
#endif
#endif /* defined(UKERNEL) */
#include "afsint.h"
#else /* KERNEL */
# include <rx/xdr.h>
# include "afsint.h"
#endif /* KERNEL */

#ifdef KERNEL
#define	NVALLOC(a)	osi_Alloc(a)
#define	NVFREE(a,b)	osi_Free(a,b)
#else /* KERNEL */
#define MAXBS	2048		/* try to avoid horrible allocs */
static afs_int32 bslosers = 0;
#define	NVALLOC(a)	malloc(a)
#define	NVFREE(a,b)	free(a)
#endif /* KERNEL */

/* these things are defined in R (but not RX's) library.  For now, we add them
    only for the kernel system.  Later, when R is expunged, we'll remove the ifdef */
#ifdef KERNEL
#ifndef AFS_USR_DARWIN_ENV
#ifdef	AFS_AIXNFS11
#define	AUTH_DES 1
#endif
/*
 * Wrapper for xdr_string that can be called directly from
 * routines like clnt_call; from user-mode xdr package.
 */
#ifndef	AFS_SUN5_ENV
#ifndef AFS_HPUX110_ENV
bool_t
xdr_wrapstring(XDR * xdrs, char **cpp)
{
    if (xdr_string(xdrs, cpp, 1024)) {
	return (TRUE);
    }
    return (FALSE);
}
#endif /* AFS_HPUX110_ENV */
#endif /* AFS_SUN5_ENV */

/*
 * xdr_vector():
 *
 * XDR a fixed length array. Unlike variable-length arrays,
 * the storage of fixed length arrays is static and unfreeable.
 * > basep: base of the array
 * > size: size of the array
 * > elemsize: size of each element
 * > xdr_elem: routine to XDR each element
 */
bool_t
xdr_vector(XDR * xdrs, char *basep, u_int nelem,
	   u_int elemsize, xdrproc_t xdr_elem)
{
    u_int i;
    char *elptr;

    elptr = basep;
    for (i = 0; i < nelem; i++) {
	if (!(*xdr_elem) (xdrs, elptr, (u_int) (~0))) {
	    return (FALSE);
	}
	elptr += elemsize;
    }
    return (TRUE);
}
#endif
#endif /* KERNEL */

#ifndef KERNEL
bool_t
xdr_CBS(XDR * x, struct CBS * abbs)
{
    afs_int32 len;
    if (x->x_op == XDR_FREE) {
	NVFREE(abbs->SeqBody, abbs->SeqLen);
	return TRUE;
    }

    if (x->x_op == XDR_ENCODE) {
	xdr_afs_int32(x, &abbs->SeqLen);
	xdr_opaque(x, abbs->SeqBody, abbs->SeqLen);
	return TRUE;
    } else {
	xdr_afs_int32(x, &len);
	if (len < 0 || len > MAXBS) {
	    bslosers++;
	    return FALSE;
	}
	if (!abbs->SeqBody)
	    abbs->SeqBody = (char *)NVALLOC(len);
	abbs->SeqLen = len;
	xdr_opaque(x, abbs->SeqBody, len);
	return TRUE;
    }
}

bool_t
xdr_BBS(XDR * x, struct BBS * abbs)
{
    afs_int32 maxLen, len;
    if (x->x_op == XDR_FREE) {
	NVFREE(abbs->SeqBody, abbs->MaxSeqLen);
	return TRUE;
    }

    if (x->x_op == XDR_ENCODE) {
	xdr_afs_int32(x, &abbs->MaxSeqLen);
	xdr_afs_int32(x, &abbs->SeqLen);
	xdr_opaque(x, abbs->SeqBody, abbs->SeqLen);
	return TRUE;
    } else {
	xdr_afs_int32(x, &maxLen);
	xdr_afs_int32(x, &len);
	if (len < 0 || len > MAXBS || len > maxLen) {
	    bslosers++;
	    return FALSE;
	}
	if (!abbs->SeqBody)
	    abbs->SeqBody = (char *)NVALLOC(maxLen);
	abbs->MaxSeqLen = maxLen;
	abbs->SeqLen = len;
	xdr_opaque(x, abbs->SeqBody, len);
	return TRUE;
    }
}

bool_t
xdr_AFSAccessList(XDR * x, AFSAccessList * abbs)
{
    afs_int32 maxLen, len;
    if (x->x_op == XDR_FREE) {
	NVFREE(abbs->SeqBody, abbs->MaxSeqLen);
	return TRUE;
    }

    if (x->x_op == XDR_ENCODE) {
	xdr_afs_int32(x, &abbs->MaxSeqLen);
	xdr_afs_int32(x, &abbs->SeqLen);
	xdr_opaque(x, abbs->SeqBody, abbs->SeqLen);
	return TRUE;
    } else {
	xdr_afs_int32(x, &maxLen);
	xdr_afs_int32(x, &len);
	if (len < 0 || len > MAXBS || len > maxLen) {
	    bslosers++;
	    return FALSE;
	}
	if (!abbs->SeqBody)
	    abbs->SeqBody = (char *)NVALLOC(maxLen);
	abbs->MaxSeqLen = maxLen;
	abbs->SeqLen = len;
	xdr_opaque(x, abbs->SeqBody, len);
	return TRUE;
    }
}
#endif /* KERNEL */
