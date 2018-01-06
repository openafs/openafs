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
#include <afs/param.h>

#ifdef KERNEL
# include "afs/sysincludes.h"
# ifndef UKERNEL
#  include "h/types.h"
#  include "h/uio.h"
#  ifdef AFS_LINUX20_ENV
#   include "h/socket.h"
#  else
#   include "rpc/types.h"
#  endif
#  ifdef AFS_LINUX22_ENV
#   ifndef quad_t
#    define quad_t __quad_t
#    define u_quad_t __u_quad_t
#   endif
#  endif
#  include "netinet/in.h"
# endif /* !UKERNEL */
#else /* KERNEL */
# include <roken.h>
#endif /* KERNEL */

#include "rx.h"
#include "xdr.h"

/* Static prototypes */
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
    /* Windows does not support labeled assigments */
    xdrrx_getint32,	/* deserialize an afs_int32 */
    xdrrx_putint32,	/* serialize an afs_int32 */
    xdrrx_getbytes,	/* deserialize counted bytes */
    xdrrx_putbytes,	/* serialize counted bytes */
    NULL,		/* get offset in the stream: not supported. */
    NULL,		/* set offset in the stream: not supported. */
    xdrrx_inline,	/* prime stream for inline macros */
    NULL,		/* destroy stream */
#else
    .x_getint32 = xdrrx_getint32,	/* deserialize an afs_int32 */
    .x_putint32 = xdrrx_putint32,	/* serialize an afs_int32 */
    .x_getbytes = xdrrx_getbytes,	/* deserialize counted bytes */
    .x_putbytes = xdrrx_putbytes,	/* serialize counted bytes */
    .x_getpostn = NULL,		/* get offset in the stream: not supported. */
    .x_setpostn = NULL,		/* set offset in the stream: not supported. */
    .x_inline = xdrrx_inline,		/* prime stream for inline macros */
    .x_destroy = NULL,			/* destroy stream */
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
