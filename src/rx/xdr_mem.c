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

RCSID
    ("$Header$");

#ifndef	NeXT

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

#include "xdr.h"
#ifndef AFS_NT40_ENV
#include <netinet/in.h>
#else
#include <limits.h>
#endif

static bool_t xdrmem_getint32();
static bool_t xdrmem_putint32();
static bool_t xdrmem_getbytes();
static bool_t xdrmem_putbytes();
static u_int xdrmem_getpos();
static bool_t xdrmem_setpos();
static afs_int32 *xdrmem_inline();
static void xdrmem_destroy();

static struct xdr_ops xdrmem_ops = {
    xdrmem_getint32,
    xdrmem_putint32,
    xdrmem_getbytes,
    xdrmem_putbytes,
    xdrmem_getpos,
    xdrmem_setpos,
    xdrmem_inline,
    xdrmem_destroy
};

/*
 * The procedure xdrmem_create initializes a stream descriptor for a
 * memory buffer.  
 */
void
xdrmem_create(register XDR * xdrs, caddr_t addr, u_int size, enum xdr_op op)
{
    xdrs->x_op = op;
    xdrs->x_ops = &xdrmem_ops;
    xdrs->x_private = xdrs->x_base = addr;
    xdrs->x_handy = (size > INT_MAX) ? INT_MAX : size;	/* XXX */
}

static void
xdrmem_destroy(void)
{
}

static bool_t
xdrmem_getint32(register XDR * xdrs, afs_int32 * lp)
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
xdrmem_putint32(register XDR * xdrs, afs_int32 * lp)
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
xdrmem_getbytes(register XDR * xdrs, caddr_t addr, register u_int len)
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
xdrmem_putbytes(register XDR * xdrs, caddr_t addr, register u_int len)
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
xdrmem_getpos(register XDR * xdrs)
{
    return ((u_int)(xdrs->x_private - xdrs->x_base));
}

static bool_t
xdrmem_setpos(register XDR * xdrs, u_int pos)
{
    register caddr_t newaddr = xdrs->x_base + pos;
    register caddr_t lastaddr = xdrs->x_private + xdrs->x_handy;

    if (newaddr > lastaddr)
	return (FALSE);
    xdrs->x_private = newaddr;
    xdrs->x_handy = (int)(lastaddr - newaddr);
    return (TRUE);
}

static afs_int32 *
xdrmem_inline(register XDR * xdrs, int len)
{
    afs_int32 *buf = 0;

    if (len >= 0 && xdrs->x_handy >= len) {
	xdrs->x_handy -= len;
	buf = (afs_int32 *) xdrs->x_private;
	xdrs->x_private += len;
    }
    return (buf);
}
#endif /* NeXT */
