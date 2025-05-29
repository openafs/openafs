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

/*
 * xdr_mem.h, XDR implementation using memory buffers.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * If you have some data to be interpreted as external data representation
 * or to be converted to external data representation in a memory buffer,
 * then this is the package for you.
 *
 */

#ifdef KERNEL
# include "afs/sysincludes.h"
#else
# include <roken.h>
# include <limits.h>
#endif

#include "xdr.h"

static bool_t xdrmem_getint32(XDR *, afs_int32 *);
static bool_t xdrmem_putint32(XDR *, afs_int32 *);
static bool_t xdrmem_getbytes(XDR *, caddr_t, u_int);
static bool_t xdrmem_putbytes(XDR *, caddr_t, u_int);
static u_int xdrmem_getpos(XDR *);
static bool_t xdrmem_setpos(XDR *, u_int);
static afs_int32 *xdrmem_inline(XDR *, u_int);
static void xdrmem_destroy(XDR *);

static struct xdr_ops xdrmem_ops = {
    AFS_STRUCT_INIT(.x_getint32, xdrmem_getint32), /* deserialize an afs_int32 */
    AFS_STRUCT_INIT(.x_putint32, xdrmem_putint32), /* serialize an afs_int32 */
    AFS_STRUCT_INIT(.x_getbytes, xdrmem_getbytes), /* deserialize counted bytes */
    AFS_STRUCT_INIT(.x_putbytes, xdrmem_putbytes), /* serialize counted bytes */
    AFS_STRUCT_INIT(.x_getpostn, xdrmem_getpos),   /* get offset in the stream: not supported. */
    AFS_STRUCT_INIT(.x_setpostn, xdrmem_setpos),   /* set offset in the stream: not supported. */
    AFS_STRUCT_INIT(.x_inline,	 xdrmem_inline),   /* prime stream for inline macros */
    AFS_STRUCT_INIT(.x_destroy,	 xdrmem_destroy),  /* destroy stream */
};

/*
 * The procedure xdrmem_create initializes a stream descriptor for a
 * memory buffer.
 */
void
xdrmem_create(XDR * xdrs, caddr_t addr, u_int size, enum xdr_op op)
{
    xdrs->x_op = op;
    xdrs->x_ops = &xdrmem_ops;
    xdrs->x_private = xdrs->x_base = addr;
    xdrs->x_handy = (size > INT_MAX) ? INT_MAX : size;	/* XXX */
}

static void
xdrmem_destroy(XDR *xdrs)
{
}

static bool_t
xdrmem_getint32(XDR *xdrs, afs_int32 * lp)
{
    if (xdrs->x_handy < sizeof(afs_int32))
	return (FALSE);
    else
	xdrs->x_handy -= sizeof(afs_int32);
    *lp = ntohl(*((afs_int32 *) (xdrs->x_private)));
    xdrs->x_private += sizeof(afs_int32);
    return (TRUE);
}

static bool_t
xdrmem_putint32(XDR *xdrs, afs_int32 * lp)
{
    if (xdrs->x_handy < sizeof(afs_int32))
	return (FALSE);
    else
	xdrs->x_handy -= sizeof(afs_int32);
    *(afs_int32 *) xdrs->x_private = htonl(*lp);
    xdrs->x_private += sizeof(afs_int32);
    return (TRUE);
}

static bool_t
xdrmem_getbytes(XDR *xdrs, caddr_t addr, u_int len)
{
    if (xdrs->x_handy < len)
	return (FALSE);
    else
	xdrs->x_handy -= len;
    memcpy(addr, xdrs->x_private, len);
    xdrs->x_private += len;
    return (TRUE);
}

static bool_t
xdrmem_putbytes(XDR *xdrs, caddr_t addr, u_int len)
{
    if (xdrs->x_handy < len)
	return (FALSE);
    else
	xdrs->x_handy -= len;
    memcpy(xdrs->x_private, addr, len);
    xdrs->x_private += len;
    return (TRUE);
}

static u_int
xdrmem_getpos(XDR *xdrs)
{
    return ((u_int)(xdrs->x_private - xdrs->x_base));
}

static bool_t
xdrmem_setpos(XDR *xdrs, u_int pos)
{
    caddr_t newaddr = xdrs->x_base + pos;
    caddr_t lastaddr = xdrs->x_private + xdrs->x_handy;

    if (newaddr > lastaddr)
	return (FALSE);
    xdrs->x_private = newaddr;
    xdrs->x_handy = (int)(lastaddr - newaddr);
    return (TRUE);
}

static afs_int32 *
xdrmem_inline(XDR *xdrs, u_int len)
{
    afs_int32 *buf = 0;

    if (xdrs->x_handy >= len) {
	xdrs->x_handy -= len;
	buf = (afs_int32 *) xdrs->x_private;
	xdrs->x_private += len;
    }
    return (buf);
}
