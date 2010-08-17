/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#include <afsconfig.h>
#include <afs/param.h>


#ifndef	NeXT

/*
 * xdr_float.c, Generic XDR routines impelmentation.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * These are the "floating point" xdr routines used to (de)serialize
 * most common data items.  See xdr.h for more info on the interface to
 * xdr.
 */

#include "xdr.h"
#include <stdio.h>

/*
 * NB: Not portable.
 * Platforms other than Windows/NT should use the XDR routines
 * in the system libraries.
 */

bool_t
xdr_float(XDR * xdrs, float *fp)
{
#ifdef AFS_NT40_ENV
    return (FALSE);
#else
    switch (xdrs->x_op) {

    case XDR_ENCODE:
	return (XDR_PUTINT32(xdrs, (afs_int32 *) fp));

    case XDR_DECODE:
	return (XDR_GETINT32(xdrs, (afs_int32 *) fp));

    case XDR_FREE:
	return (TRUE);
    }
    return (FALSE);
#endif
}

bool_t
xdr_double(XDR * xdrs, double *dp)
{
#ifdef AFS_NT40_ENV
    return (FALSE);
#else
    afs_int32 *ip;
    switch (xdrs->x_op) {

    case XDR_ENCODE:
	ip = (afs_int32 *) (dp);
	return (XDR_PUTINT32(xdrs, *(ip + 1)) && XDR_PUTINT32(xdrs, *ip));

    case XDR_DECODE:
	ip = (afs_int32 *) (dp);
	return (XDR_GETINT32(xdrs, *(ip + 1)) && XDR_GETINT32(xdrs, *ip));

    case XDR_FREE:
	return (TRUE);
    }
    return (FALSE);
#endif
}

#endif /* NeXT */
