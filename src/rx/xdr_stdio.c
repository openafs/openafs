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
 * xdr_stdio.c, XDR implementation on standard i/o file.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * This set of routines implements a XDR on a stdio stream.
 * XDR_ENCODE serializes onto the stream, XDR_DECODE de-serializes
 * from the stream.
 */

#include <stdio.h>
#include "xdr.h"

static bool_t xdrstdio_getint32();
static bool_t xdrstdio_putint32();
static bool_t xdrstdio_getbytes();
static bool_t xdrstdio_putbytes();
static u_int xdrstdio_getpos();
static bool_t xdrstdio_setpos();
static afs_int32 *xdrstdio_inline();
static void xdrstdio_destroy();

/*
 * Ops vector for stdio type XDR
 */
static struct xdr_ops xdrstdio_ops = {
#ifdef AFS_NT40_ENV
#ifdef AFS_XDR_64BITOPS
    NULL,
    NULL,
#endif
    /* Windows does not support labeled assignments */
#if !(defined(KERNEL) && defined(AFS_SUN57_ENV))
    xdrstdio_getint32,	        /* deserialize an afs_int32 */
    xdrstdio_putint32,	        /* serialize an afs_int32 */
#endif
    xdrstdio_getbytes,	        /* deserialize counted bytes */
    xdrstdio_putbytes,	        /* serialize counted bytes */
    xdrstdio_getpos,	        /* get offset in the stream */
    xdrstdio_setpos,	        /* set offset in the stream */
    xdrstdio_inline,	        /* prime stream for inline macros */
    xdrstdio_destroy,	        /* destroy stream */
#if (defined(KERNEL) && defined(AFS_SUN57_ENV))
    NULL,
    xdrstdio_getint32,    /* deserialize an afs_int32 */
    xdrstdio_putint32,    /* serialize an afs_int32 */
#endif
#else
#ifdef AFS_XDR_64BITOPS
    .x_getint64 = NULL,
    .x_putint64 = NULL,
#endif
    .x_getint32 = xdrstdio_getint32,	/* deserialize an afs_int32 */
    .x_putint32 = xdrstdio_putint32,	/* serialize an afs_int32 */
    .x_getbytes = xdrstdio_getbytes,	/* deserialize counted bytes */
    .x_putbytes = xdrstdio_putbytes,	/* serialize counted bytes */
    .x_getpos = xdrstdio_getpos,	/* get offset in the stream */
    .x_setpos = xdrstdio_setpos,	/* set offset in the stream */
    .x_inline = xdrstdio_inline,	/* prime stream for inline macros */
    .x_destroy = xdrstdio_destroy	/* destroy stream */
#endif
};

/*
 * Initialize a stdio xdr stream.
 * Sets the xdr stream handle xdrs for use on the stream file.
 * Operation flag is set to op.
 */
void
xdrstdio_create(xdrs, file, op)
     XDR *xdrs;
     FILE *file;
     enum xdr_op op;
{

    xdrs->x_op = op;
    xdrs->x_ops = &xdrstdio_ops;
    xdrs->x_private = (caddr_t) file;
    xdrs->x_handy = 0;
    xdrs->x_base = 0;
}

/*
 * Destroy a stdio xdr stream.
 * Cleans up the xdr stream handle xdrs previously set up by xdrstdio_create.
 */
static void
xdrstdio_destroy(xdrs)
     XDR *xdrs;
{
    (void)fflush((FILE *) xdrs->x_private);
    /* xx should we close the file ?? */
};

static bool_t
xdrstdio_getint32(xdrs, lp)
     XDR *xdrs;
     afs_int32 *lp;
{

    if (fread((caddr_t) lp, sizeof(afs_int32), 1, (FILE *) xdrs->x_private) !=
	1)
	return (FALSE);
#ifndef mc68000
    *lp = ntohl(*lp);
#endif
    return (TRUE);
}

static bool_t
xdrstdio_putint32(xdrs, lp)
     XDR *xdrs;
     afs_int32 *lp;
{

#ifndef mc68000
    afs_int32 mycopy = htonl(*lp);
    lp = &mycopy;
#endif
    if (fwrite((caddr_t) lp, sizeof(afs_int32), 1, (FILE *) xdrs->x_private)
	!= 1)
	return (FALSE);
    return (TRUE);
}

static bool_t
xdrstdio_getbytes(xdrs, addr, len)
     XDR *xdrs;
     caddr_t addr;
     u_int len;
{

    if ((len != 0)
	&& (fread(addr, (int)len, 1, (FILE *) xdrs->x_private) != 1))
	return (FALSE);
    return (TRUE);
}

static bool_t
xdrstdio_putbytes(xdrs, addr, len)
     XDR *xdrs;
     caddr_t addr;
     u_int len;
{

    if ((len != 0)
	&& (fwrite(addr, (int)len, 1, (FILE *) xdrs->x_private) != 1))
	return (FALSE);
    return (TRUE);
}

static u_int
xdrstdio_getpos(xdrs)
     XDR *xdrs;
{

    return ((u_int) ftell((FILE *) xdrs->x_private));
}

static bool_t
xdrstdio_setpos(xdrs, pos)
     XDR *xdrs;
     u_int pos;
{

    return ((fseek((FILE *) xdrs->x_private, (afs_int32) pos, 0) <
	     0) ? FALSE : TRUE);
}

static afs_int32 *
xdrstdio_inline(xdrs, len)
     XDR *xdrs;
     u_int len;
{

    /*
     * Must do some work to implement this: must insure
     * enough data in the underlying stdio buffer,
     * that the buffer is aligned so that we can indirect through a
     * afs_int32 *, and stuff this pointer in xdrs->x_buf.  Doing
     * a fread or fwrite to a scratch buffer would defeat
     * most of the gains to be had here and require storage
     * management on this buffer, so we don't do this.
     */
    return (NULL);
}
#endif /* NeXT */
