/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * xdr_afsuuid.c, XDR routine for built in afsUUID data type.
 */

#include <afsconfig.h>
#include "afs/param.h"


#if defined(KERNEL) && !defined(UKERNEL)
#ifdef AFS_LINUX20_ENV
#include "h/string.h"
#if 0
#define bzero(A, C) memset((A), 0, (C))
#endif
#else
#include <sys/param.h>
#include <sys/systm.h>
#endif
#else
#include <stdio.h>
#endif
#include "xdr.h"

int
xdr_afsUUID(XDR * xdrs, afsUUID * objp)
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
    /* Cast needed here because xdrproc_t officially takes 3 args :-( */
    if (!xdr_vector(xdrs, (char *)objp->node, 6, sizeof(char), (xdrproc_t)xdr_char)) {
	return (FALSE);
    }
    return (TRUE);
}
