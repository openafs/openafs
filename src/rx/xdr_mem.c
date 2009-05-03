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
#ifndef	NeXT

#include <afsconfig.h>

#ifdef KERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

#if defined(KERNEL)
#include "afs/sysincludes.h"    /*Standard vendor system headers */
#if !defined(UKERNEL)
#include "h/types.h"
#include "h/uio.h"
#ifdef	AFS_OSF_ENV
#include <net/net_globals.h>
#endif	/* AFS_OSF_ENV */
#ifdef AFS_LINUX20_ENV
#include "h/socket.h"
#else
#include "rpc/types.h"
#endif
#ifdef AFS_OSF_ENV
#undef kmem_alloc
#undef kmem_free
#undef mem_alloc
#undef mem_free
#undef register
#endif	/* AFS_OSF_ENV */
#ifdef AFS_LINUX22_ENV
#ifndef quad_t
#define quad_t __quad_t
#define u_quad_t __u_quad_t
#endif
#endif
#include "rx/xdr.h"
#else	/* !UKERNEL */
#include "rpc/types.h"
#include "rpc/xdr.h"
#endif	/* !UKERNEL */

#else	/* KERNEL */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#ifndef AFS_NT40_ENV
#include <netinet/in.h>
#endif
#include "xdr.h"
#endif	/* KERNEL */

#ifdef AFS_NT40_ENV
#include <limits.h>
#endif

/*
 * this section should be shared with xdr_rx.c
 */
#if defined(KERNEL)
/*
 * kernel version needs to agree with <rpc/xdr.h>
 * except on Linux which does XDR differently from everyone else
 */
# if defined(AFS_LINUX20_ENV) && !defined(UKERNEL)
#  define AFS_XDRS_T void *
# else
#  define AFS_XDRS_T XDR *
# endif
# if defined(AFS_SUN57_ENV)
#  define AFS_RPC_INLINE_T rpc_inline_t
# elif defined(AFS_DUX40_ENV)
#  define AFS_RPC_INLINE_T int
# elif defined(AFS_LINUX20_ENV) && !defined(UKERNEL)
#  define AFS_RPC_INLINE_T afs_int32
# elif defined(AFS_LINUX20_ENV)
#  define AFS_RPC_INLINE_T int32_t *
# else
#  define AFS_RPC_INLINE_T long
# endif
#else	/* KERNEL */
/*
 * user version needs to agree with "xdr.h" -- ie, <rx/xdr.h>
 */
# define AFS_XDRS_T void *
# define AFS_RPC_INLINE_T afs_int32
#endif

#ifndef INT_MAX
#define INT_MAX	(~0u>>1)
#endif

RCSID
    ("$Header$");

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

static bool_t xdrmem_getint32();
static bool_t xdrmem_putint32();
static bool_t xdrmem_getbytes();
static bool_t xdrmem_putbytes();
static u_int xdrmem_getpos();
static bool_t xdrmem_setpos();
static AFS_RPC_INLINE_T *xdrmem_inline();
static void xdrmem_destroy();

static struct xdr_ops xdrmem_ops = {
#if defined(KERNEL) && ((defined(AFS_SGI61_ENV) && (_MIPS_SZLONG != _MIPS_SZINT)) || defined(AFS_HPUX_64BIT_ENV))
    xdrmem_getint64,
    xdrmem_putint64,
#endif
#if !(defined(KERNEL) && defined(AFS_SUN57_ENV))
    xdrmem_getint32,
    xdrmem_putint32,
#endif
    xdrmem_getbytes,
    xdrmem_putbytes,
    xdrmem_getpos,
    xdrmem_setpos,
    xdrmem_inline,
    xdrmem_destroy,
#if (defined(KERNEL) && defined(AFS_SUN57_ENV))
    0,
    xdrmem_getint32,
    xdrmem_putint32,
#endif
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

#if (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG != _MIPS_SZINT)) || defined(AFS_HPUX_64BIT_ENV)
static bool_t
xdrmem_getint64(register XDR * xdrs, long * lp)
{
    if (xdrs->x_handy < sizeof(long))
	return (FALSE);
    else
	xdrs->x_handy -= sizeof(long);
    *lp = ntohl(0[(afs_int32 *) xdrs->x_private]);
    *lp <<= 32;
    *lp += ntohl(1[(afs_int32 *) xdrs->x_private]);
    xdrs->x_private += sizeof(long);
    return (TRUE);
}

static bool_t
xdrmem_putint64(register XDR * xdrs, long * lp)
{
    afs_int32 t;
    if (xdrs->x_handy < sizeof(long))
	return (FALSE);
    else
	xdrs->x_handy -= sizeof(long);
    t = (*lp >> 32); t = htonl(t);
    0[(afs_int32 *) xdrs->x_private] = t;
    t = (*lp); t = htonl(t);
    1[(afs_int32 *) xdrs->x_private] = t;
    xdrs->x_private += sizeof(long);
    return (TRUE);
}
#endif

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
    return ((char *)xdrs->x_private - (char *)xdrs->x_base);
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

static AFS_RPC_INLINE_T *
xdrmem_inline(register XDR * xdrs, int len)
{
    AFS_RPC_INLINE_T * buf = 0;

    if (len >= 0 && xdrs->x_handy >= len) {
	xdrs->x_handy -= len;
	buf = (AFS_RPC_INLINE_T *) xdrs->x_private;
	xdrs->x_private += len;
    }
    return (buf);
}
#endif /* NeXT */
