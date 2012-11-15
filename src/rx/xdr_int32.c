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
#ifdef KERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif


#ifndef	NeXT

#if defined(KERNEL) && !defined(UKERNEL)
# if !defined(AFS_LINUX26_ENV)
#  include <sys/param.h>
# endif
#else
# include <stdio.h>
#endif
#include "xdr.h"

/*
 * XDR afs_int32 integers
 * same as xdr_u_long - open coded to save a proc call!
 */
bool_t
xdr_afs_int32(XDR * xdrs, afs_int32 * lp)
{

    if (xdrs->x_op == XDR_ENCODE)
	return (XDR_PUTINT32(xdrs, lp));

    if (xdrs->x_op == XDR_DECODE)
	return (XDR_GETINT32(xdrs, lp));

    if (xdrs->x_op == XDR_FREE)
	return (TRUE);

    return (FALSE);
}

/*
 * XDR unsigned afs_int32 integers
 * same as xdr_long - open coded to save a proc call!
 */
bool_t
xdr_afs_uint32(XDR * xdrs, afs_uint32 * ulp)
{

    if (xdrs->x_op == XDR_DECODE)
	return (XDR_GETINT32(xdrs, (afs_int32 *)ulp));
    if (xdrs->x_op == XDR_ENCODE)
	return (XDR_PUTINT32(xdrs, (afs_int32 *)ulp));
    if (xdrs->x_op == XDR_FREE)
	return (TRUE);
    return (FALSE);
}

#endif /* NeXT */
