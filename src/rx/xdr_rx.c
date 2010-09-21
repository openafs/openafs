/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * xdr_rx.c.  XDR using RX.
 */

#include <afsconfig.h>
#ifdef	KERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif


#ifdef KERNEL
# include "afs/sysincludes.h"
# ifndef UKERNEL
#  include "h/types.h"
#  include "h/uio.h"
#  ifdef AFS_OSF_ENV
#   include <net/net_globals.h>
#  endif /* AFS_OSF_ENV */
#  ifdef AFS_LINUX20_ENV
#   include "h/socket.h"
#  else
#   include "rpc/types.h"
#  endif
#  ifdef  AFS_OSF_ENV
#   undef kmem_alloc
#   undef kmem_free
#   undef mem_alloc
#   undef mem_free
#  endif /* AFS_OSF_ENV */
#  ifdef AFS_LINUX22_ENV
#   ifndef quad_t
#    define quad_t __quad_t
#    define u_quad_t __u_quad_t
#   endif
#  endif
#  include "rx/xdr.h"
#  include "netinet/in.h"
# endif /* !UKERNEL */
# include "rx/xdr.h"
# include "rx/rx.h"

#else /* KERNEL */
# include <sys/types.h>
# include <stdio.h>
# ifndef AFS_NT40_ENV
#  include <netinet/in.h>
# endif
# include "rx.h"
# include "xdr.h"
#endif /* KERNEL */

/* Static prototypes */
#ifdef AFS_XDR_64BITOPS
static bool_t xdrrx_getint64(XDR *axdrs, long *lp);
static bool_t xdrrx_putint64(XDR *axdrs, long *lp);
#endif /* AFS_XDR_64BITOPS */

static bool_t xdrrx_getint32(XDR *axdrs, afs_int32 * lp);
static bool_t xdrrx_putint32(XDR *axdrs, afs_int32 * lp);
static bool_t xdrrx_getbytes(XDR *axdrs, caddr_t addr,
			     u_int len);
static bool_t xdrrx_putbytes(XDR *axdrs, caddr_t addr,
			     u_int len);
static afs_int32 *xdrrx_inline(XDR *axdrs, u_int len);


/*
 * Ops vector for stdio type XDR
 */
static struct xdr_ops xdrrx_ops = {
#ifndef HAVE_STRUCT_LABEL_SUPPORT
#ifdef AFS_XDR_64BITOPS
    xdrrx_getint64,     /* deserialize an afs_int64 */
    xdrrx_putint64,     /* serialize an afs_int64 */
#endif
    /* Windows does not support labeled assigments */
#if !(defined(KERNEL) && defined(AFS_SUN57_ENV))
    xdrrx_getint32,	/* deserialize an afs_int32 */
    xdrrx_putint32,	/* serialize an afs_int32 */
#endif
    xdrrx_getbytes,	/* deserialize counted bytes */
    xdrrx_putbytes,	/* serialize counted bytes */
    NULL,		/* get offset in the stream: not supported. */
    NULL,		/* set offset in the stream: not supported. */
    xdrrx_inline,	/* prime stream for inline macros */
    NULL,		/* destroy stream */
#if (defined(KERNEL) && defined(AFS_SUN57_ENV))
    NULL,               /* control - not implemented */
    xdrrx_getint32,     /* not supported */
    xdrrx_putint32,     /* serialize an afs_int32 */
#endif
#else
#ifdef AFS_XDR_64BITOPS
    .x_getint64 = xdrrx_getint64,
    .x_putint64 = xdrrx_putint64,
#endif /* AFS_XDR_64BITOPS */
    .x_getint32 = xdrrx_getint32,	/* deserialize an afs_int32 */
    .x_putint32 = xdrrx_putint32,	/* serialize an afs_int32 */
    .x_getbytes = xdrrx_getbytes,	/* deserialize counted bytes */
    .x_putbytes = xdrrx_putbytes,	/* serialize counted bytes */
    .x_getpostn = NULL,		/* get offset in the stream: not supported. */
    .x_setpostn = NULL,		/* set offset in the stream: not supported. */
    .x_inline = xdrrx_inline,		/* prime stream for inline macros */
    .x_destroy = NULL,			/* destroy stream */
#if defined(KERNEL) && defined(AFS_SUN57_ENV)
    .x_control = NULL,
#endif
#endif
};

/*
 * Initialize an rx xdr handle, for a given rx call.  op must be XDR_ENCODE or XDR_DECODE.
 * Call must have been returned by rx_MakeCall or rx_GetCall.
 */
void
xdrrx_create(XDR * xdrs, struct rx_call *call,
	     enum xdr_op op)
{
    xdrs->x_op = op;
    xdrs->x_ops = &xdrrx_ops;
    xdrs->x_private = (caddr_t) call;
}

#if	defined(KERNEL) && defined(AFS_AIX32_ENV)
#define STACK_TO_PIN	2*PAGESIZE	/* 8K */
int rx_pin_failed = 0;
#endif

#ifdef AFS_XDR_64BITOPS
static bool_t
xdrrx_getint64(XDR *axdrs, long *lp)
{
    XDR * xdrs = (XDR *)axdrs;
    struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);
    afs_int32 i;

    if (rx_Read32(call, &i) == sizeof(i)) {
	*lp = ntohl(i);
	return TRUE;
    }
    return FALSE;
}

static bool_t
xdrrx_putint64(XDR *axdrs, long *lp)
{
    XDR * xdrs = (XDR *)axdrs;
    afs_int32 code, i = htonl(*lp);
    struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);

    code = (rx_Write32(call, &i) == sizeof(i));
    return code;
}
#endif /* AFS_XDR_64BITOPS */

static bool_t
xdrrx_getint32(XDR *axdrs, afs_int32 * lp)
{
    afs_int32 l;
    XDR * xdrs = (XDR *)axdrs;
    struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);
#if	defined(KERNEL) && defined(AFS_AIX32_ENV)
    char *saddr = (char *)&l;
    saddr -= STACK_TO_PIN;
    /*
     * Hack of hacks: Aix3.2 only guarantees that the next 2K of stack in pinned. Under
     * splnet (disables interrupts), which is set throughout rx, we can't swap in stack
     * pages if we need so we panic. Since sometimes, under splnet, we'll use more than
     * 2K stack we could try to bring the next few stack pages in here before we call the rx
     * layer. Of course this doesn't guarantee that those stack pages won't be swapped
     * out between here and calling splnet. So we now pin (and unpin) them instead to
     * guarantee that they remain there.
     */
    if (pin(saddr, STACK_TO_PIN)) {
	/* XXX There's little we can do by continue XXX */
	saddr = NULL;
	rx_pin_failed++;
    }
#endif
    if (rx_Read32(call, &l) == sizeof(l)) {
	*lp = ntohl(l);
#if	defined(KERNEL) && defined(AFS_AIX32_ENV)
	if (saddr)
	    unpin(saddr, STACK_TO_PIN);
#endif
	return TRUE;
    }
#if	defined(KERNEL) && defined(AFS_AIX32_ENV)
    if (saddr)
	unpin(saddr, STACK_TO_PIN);
#endif
    return FALSE;
}

static bool_t
xdrrx_putint32(XDR *axdrs, afs_int32 * lp)
{
    afs_int32 code, l = htonl(*lp);
    XDR * xdrs = (XDR *)axdrs;
    struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);
#if	defined(KERNEL) && defined(AFS_AIX32_ENV)
    char *saddr = (char *)&code;
    saddr -= STACK_TO_PIN;
    /*
     * Hack of hacks: Aix3.2 only guarantees that the next 2K of stack in pinned. Under
     * splnet (disables interrupts), which is set throughout rx, we can't swap in stack
     * pages if we need so we panic. Since sometimes, under splnet, we'll use more than
     * 2K stack we could try to bring the next few stack pages in here before we call the rx
     * layer. Of course this doesn't guarantee that those stack pages won't be swapped
     * out between here and calling splnet. So we now pin (and unpin) them instead to
     * guarantee that they remain there.
     */
    if (pin(saddr, STACK_TO_PIN)) {
	saddr = NULL;
	rx_pin_failed++;
    }
#endif
    code = (rx_Write32(call, &l) == sizeof(l));
#if	defined(KERNEL) && defined(AFS_AIX32_ENV)
    if (saddr)
	unpin(saddr, STACK_TO_PIN);
#endif
    return code;
}

static bool_t
xdrrx_getbytes(XDR *axdrs, caddr_t addr, u_int len)
{
    afs_int32 code;
    XDR * xdrs = (XDR *)axdrs;
    struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);
#if	defined(KERNEL) && defined(AFS_AIX32_ENV)
    char *saddr = (char *)&code;
    saddr -= STACK_TO_PIN;
    /*
     * Hack of hacks: Aix3.2 only guarantees that the next 2K of stack in pinned. Under
     * splnet (disables interrupts), which is set throughout rx, we can't swap in stack
     * pages if we need so we panic. Since sometimes, under splnet, we'll use more than
     * 2K stack we could try to bring the next few stack pages in here before we call the rx
     * layer. Of course this doesn't guarantee that those stack pages won't be swapped
     * out between here and calling splnet. So we now pin (and unpin) them instead to
     * guarantee that they remain there.
     */
    if (pin(saddr, STACK_TO_PIN)) {
	/* XXX There's little we can do by continue XXX */
	saddr = NULL;
	rx_pin_failed++;
    }
#endif
    code = (rx_Read(call, addr, len) == len);
#if	defined(KERNEL) && defined(AFS_AIX32_ENV)
    if (saddr)
	unpin(saddr, STACK_TO_PIN);
#endif
    return code;
}

static bool_t
xdrrx_putbytes(XDR *axdrs, caddr_t addr, u_int len)
{
    afs_int32 code;
    XDR * xdrs = (XDR *)axdrs;
    struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);
#if	defined(KERNEL) && defined(AFS_AIX32_ENV)
    char *saddr = (char *)&code;
    saddr -= STACK_TO_PIN;
    /*
     * Hack of hacks: Aix3.2 only guarantees that the next 2K of stack in pinned. Under
     * splnet (disables interrupts), which is set throughout rx, we can't swap in stack
     * pages if we need so we panic. Since sometimes, under splnet, we'll use more than
     * 2K stack we could try to bring the next few stack pages in here before we call the rx
     * layer. Of course this doesn't guarantee that those stack pages won't be swapped
     * out between here and calling splnet. So we now pin (and unpin) them instead to
     * guarantee that they remain there.
     */
    if (pin(saddr, STACK_TO_PIN)) {
	/* XXX There's little we can do by continue XXX */
	saddr = NULL;
	rx_pin_failed++;
    }
#endif
    code = (rx_Write(call, addr, len) == len);
#if	defined(KERNEL) && defined(AFS_AIX32_ENV)
    if (saddr)
	unpin(saddr, STACK_TO_PIN);
#endif
    return code;
}

#ifdef undef			/* not used */
static u_int
xdrrx_getpos(XDR * xdrs)
{
    /* Not supported.  What error code should we return? (It doesn't matter:  it will never be called, anyway!) */
    return -1;
}

static bool_t
xdrrx_setpos(XDR * xdrs, u_int pos)
{
    /* Not supported */
    return FALSE;
}
#endif

static afs_int32 *
xdrrx_inline(XDR *axdrs, u_int len)
{
    /* I don't know what this routine is supposed to do, but the stdio module returns null, so we will, too */
    return (0);
}
