
/*
 * xdr_afsuuid.c, XDR routine for built in afsUUID data type.
 */

#if defined(KERNEL) && !defined(UKERNEL)
#include "afs/param.h"
#ifdef AFS_LINUX20_ENV
#include "../h/string.h"
#define bzero(A,C) memset((A), 0, (C))
#else
#include <sys/param.h>
#include <sys/systm.h>
#endif
#else
#include <stdio.h>
#endif
#include "xdr.h"
#if defined(KERNEL) && !defined(UKERNEL)
#ifdef        AFS_DEC_ENV
#include <afs/longc_procs.h>
#endif
#endif

#ifndef lint
static char sccsid[] = "@(#)xdr_array.c 1.1 86/02/03 Copyr 1984 Sun Micro";
#endif

int
xdr_afsUUID(xdrs, objp)
	XDR *xdrs;
	afsUUID *objp;
{
	if (!xdr_afs_uint32(xdrs, &objp->time_low)) {
		return (FALSE);
	}
	if (!xdr_u_short(xdrs, &objp->time_mid)) {
		return (FALSE);
	}
	if (!xdr_u_short(xdrs, &objp->time_hi_and_version)) {
		return (FALSE);
	}
	if (!xdr_char(xdrs, &objp->clock_seq_hi_and_reserved)) {
		return (FALSE);
	}
	if (!xdr_char(xdrs, &objp->clock_seq_low)) {
		return (FALSE);
	}
	if (!xdr_vector(xdrs, (char *)objp->node, 6, sizeof(char), xdr_char)) {
		return (FALSE);
	}
	return (TRUE);
}
