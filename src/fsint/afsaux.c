
#ifndef lint
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
#ifdef KERNEL
#include "../afs/param.h"
#if defined(UKERNEL)
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../rx/xdr.h"
#else /* defined(UKERNEL) */
#if defined(AFS_ALPHA_ENV) || defined(AFS_LINUX20_ENV)
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#else
#include "../h/types.h"
#include "../rpc/types.h"
#include "../rpc/xdr.h"
#endif
#if !defined(AFS_ALPHA_ENV)
#ifndef	XDR_GETINT32
#define	XDR_GETINT32	XDR_GETLONG
#endif
#ifndef	XDR_PUTINT32
#define	XDR_PUTINT32	XDR_PUTLONG
#endif
#endif
#ifndef AFS_LINUX22_ENV
#include "../rpc/auth.h"
#endif
#endif /* defined(UKERNEL) */
#include "../afsint/afsint.h"
#else /* KERNEL */
# include <afs/param.h>
# include <rx/xdr.h>
# include "afsint.h"
#endif /* KERNEL */

#define MAXBS	2048		/* try to avoid horrible allocs */
static afs_int32 bslosers = 0;

#ifdef KERNEL
#define	NVALLOC(a)	osi_Alloc(a)
#define	NVFREE(a,b)	osi_Free(a,b)
#else /* KERNEL */
#define	NVALLOC(a)	malloc(a)
#define	NVFREE(a,b)	free(a)
#endif /* KERNEL */

/* these things are defined in R (but not RX's) library.  For now, we add them
    only for the kernel system.  Later, when R is expunged, we'll remove the ifdef */
#ifdef KERNEL
#ifdef	AFS_AIXNFS11
#define	AUTH_DES 1
#endif
#if (defined(AFS_AIX_ENV) && !defined(AUTH_DES)) || (!defined(AFS_SUN_ENV)) && !defined(AFS_SGI_ENV) && !defined(AFS_ALPHA_ENV) && !defined(AFS_SUN5_ENV)
#ifndef	AFS_AIX32_ENV
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV)
/*
 * XDR chars; from user mode xdr package.
 */
bool_t
xdr_char(xdrs, sp)
	register XDR *xdrs;
	char *sp;
{
	afs_int32 l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (afs_int32) *sp;
		return (XDR_PUTINT32(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, &l)) {
			return (FALSE);
		}
		*sp = (char) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}
#endif /* AFS_HPUX110_ENV && AFS_LINUX20_ENV */
#endif 
#endif /* defined(AFS_AIX_ENV) && !defined(AUTH_DES) */


/* 
 * Wrapper for xdr_string that can be called directly from 
 * routines like clnt_call; from user-mode xdr package.
 */
#ifndef	AFS_SUN5_ENV
#ifndef AFS_HPUX110_ENV
bool_t
xdr_wrapstring(xdrs, cpp)
	XDR *xdrs;
	char **cpp;
{
	if (xdr_string(xdrs, cpp, 1024)) {
		return(TRUE);
	}
	return(FALSE);
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
xdr_vector(xdrs, basep, nelem, elemsize, xdr_elem)
	register XDR *xdrs;
	register char *basep;
	register u_int nelem;
	register u_int elemsize;
	register xdrproc_t xdr_elem;	
{
	register u_int i;
	register char *elptr;

	elptr = basep;
	for (i = 0; i < nelem; i++) {
		if (! (*xdr_elem)(xdrs, elptr, (u_int) (~0))) {
			return(FALSE);
		}
		elptr += elemsize;
	}
	return(TRUE);	
}
#endif /* KERNEL */

#ifndef KERNEL
xdr_CBS(x, abbs)
    XDR *x;
    struct CBS *abbs; {
    afs_int32 len;
    if (x->x_op == XDR_FREE) {
	NVFREE(abbs->SeqBody,abbs->SeqLen);
	return TRUE;
    }

    if (x->x_op == XDR_ENCODE) {
	xdr_afs_int32(x, &abbs->SeqLen);
	xdr_opaque(x, abbs->SeqBody, abbs->SeqLen);
	return TRUE;
    }
    else {
	xdr_afs_int32(x, &len);
	if (len < 0 || len > MAXBS) {bslosers++; return FALSE;}
	if (!abbs->SeqBody) abbs->SeqBody = (char *) NVALLOC(len);
	abbs->SeqLen = len;
	xdr_opaque(x, abbs->SeqBody, len);
	return TRUE;
    }
}

xdr_BBS(x, abbs)
    XDR *x;
    struct BBS *abbs; {
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
    }
    else {
	xdr_afs_int32(x, &maxLen);
	xdr_afs_int32(x, &len);
	if (len < 0 || len > MAXBS || len > maxLen) {bslosers++; return FALSE;}
	if (!abbs->SeqBody) abbs->SeqBody = (char *) NVALLOC(maxLen);
	abbs->MaxSeqLen = maxLen;
	abbs->SeqLen = len;
	xdr_opaque(x, abbs->SeqBody, len);
	return TRUE;
    }
}

xdr_AFSAccessList(x, abbs)
    XDR *x;
    struct BBS *abbs; {
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
    }
    else {
	xdr_afs_int32(x, &maxLen);
	xdr_afs_int32(x, &len);
	if (len < 0 || len > MAXBS || len > maxLen) {bslosers++; return FALSE;}
	if (!abbs->SeqBody) abbs->SeqBody = (char *) NVALLOC(maxLen);
	abbs->MaxSeqLen = maxLen;
	abbs->SeqLen = len;
	xdr_opaque(x, abbs->SeqBody, len);
	return TRUE;
    }
}
#endif /* KERNEL */
