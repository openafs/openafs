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

RCSID
    ("$Header$");

#ifdef KERNEL
#include "afs/sysincludes.h"
#ifndef UKERNEL
#include "h/types.h"
#include "h/uio.h"
#ifdef	AFS_OSF_ENV
#include <net/net_globals.h>
#endif /* AFS_OSF_ENV */
#ifdef AFS_LINUX20_ENV
#include "h/socket.h"
#else
#include "rpc/types.h"
#endif
#ifdef  AFS_OSF_ENV
#undef kmem_alloc
#undef kmem_free
#undef mem_alloc
#undef mem_free
#undef register
#endif /* AFS_OSF_ENV */
#ifdef AFS_LINUX22_ENV
#ifndef quad_t
#define quad_t __quad_t
#define u_quad_t __u_quad_t
#endif
#endif
#include "rx/xdr.h"
#include "netinet/in.h"
#else /* !UKERNEL */
#include "rpc/types.h"
#include "rpc/xdr.h"
#endif /* !UKERNEL */
#include "rx/rx.h"

#include "afs/longc_procs.h"

#else /* KERNEL */
#include <sys/types.h>
#include <stdio.h>
#ifndef AFS_NT40_ENV
#include <netinet/in.h>
#endif
#include "rx.h"
#include "xdr.h"
#endif /* KERNEL */

/*
 * This section should really go in the param.*.h files.
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
#else /* KERNEL */
/*
 * user version needs to agree with "xdr.h", i.e. <rx/xdr.h>
 */
#  define AFS_XDRS_T void *
#  define AFS_RPC_INLINE_T afs_int32
#endif /* KERNEL */

/* Static prototypes */
#if (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG != _MIPS_SZINT)) || defined(AFS_HPUX_64BIT_ENV)
static bool_t xdrrx_getint64(AFS_XDRS_T axdrs, long *lp);
static bool_t xdrrx_putint64(AFS_XDRS_T axdrs, long *lp);
#endif /* (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG != _MIPS_SZINT)) || defined(AFS_HPUX_64BIT_ENV) */

static bool_t xdrrx_getint32(AFS_XDRS_T axdrs, afs_int32 * lp);
static bool_t xdrrx_putint32(AFS_XDRS_T axdrs, register afs_int32 * lp);
static bool_t xdrrx_getbytes(AFS_XDRS_T axdrs, register caddr_t addr,
			     register u_int len);
static bool_t xdrrx_putbytes(AFS_XDRS_T axdrs, register caddr_t addr,
			     register u_int len);
static AFS_RPC_INLINE_T *xdrrx_inline(AFS_XDRS_T axdrs, register u_int len);


/*
 * Ops vector for stdio type XDR
 */
static struct xdr_ops xdrrx_ops = {
#if defined(KERNEL) && ((defined(AFS_SGI61_ENV) && (_MIPS_SZLONG != _MIPS_SZINT)) || defined(AFS_HPUX_64BIT_ENV))
    xdrrx_getint64,
    xdrrx_putint64,
#endif /* defined(KERNEL) && ((defined(AFS_SGI61_ENV) && (_MIPS_SZLONG != _MIPS_SZINT)) || defined(AFS_HPUX_64BIT_ENV)) */
#if !(defined(KERNEL) && defined(AFS_SUN57_ENV))
    xdrrx_getint32,		/* deserialize an afs_int32 */
    xdrrx_putint32,		/* serialize an afs_int32 */
#endif
    xdrrx_getbytes,		/* deserialize counted bytes */
    xdrrx_putbytes,		/* serialize counted bytes */
    0,				/* get offset in the stream: not supported. */
    0,				/* set offset in the stream: not supported. */
    xdrrx_inline,		/* prime stream for inline macros */
    0,				/* destroy stream */
#if defined(KERNEL) && defined(AFS_SUN57_ENV)
    0,
    xdrrx_getint32,		/* deserialize an afs_int32 */
    xdrrx_putint32,		/* serialize an afs_int32 */
#endif
};

/*
 * Initialize an rx xdr handle, for a given rx call.  op must be XDR_ENCODE or XDR_DECODE.
 * Call must have been returned by rx_MakeCall or rx_GetCall.
 */
void
xdrrx_create(register XDR * xdrs, register struct rx_call *call,
	     register enum xdr_op op)
{
    xdrs->x_op = op;
    xdrs->x_ops = &xdrrx_ops;
    xdrs->x_private = (caddr_t) call;
}

#if	defined(KERNEL) && defined(AFS_AIX32_ENV)
#define STACK_TO_PIN	2*PAGESIZE	/* 8K */
int rx_pin_failed = 0;
#endif

#if (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG != _MIPS_SZINT)) || defined(AFS_HPUX_64BIT_ENV)
static bool_t
xdrrx_getint64(AFS_XDRS_T axdrs, long *lp)
{
    XDR * xdrs = (XDR *)axdrs;
    register struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);
    afs_int32 i;

    if (rx_Read32(call, &i) == sizeof(i)) {
	*lp = ntohl(i);
	return TRUE;
    }
    return FALSE;
}

static bool_t
xdrrx_putint64(AFS_XDRS_T axdrs, long *lp)
{
    XDR * xdrs = (XDR *)axdrs;
    afs_int32 code, i = htonl(*lp);
    register struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);

    code = (rx_Write32(call, &i) == sizeof(i));
    return code;
}
#endif /* (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG != _MIPS_SZINT)) || defined(AFS_HPUX_64BIT_ENV) */

static bool_t
xdrrx_getint32(AFS_XDRS_T axdrs, afs_int32 * lp)
{
    afs_int32 l;
    XDR * xdrs = (XDR *)axdrs;
    register struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);
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
xdrrx_putint32(register AFS_XDRS_T axdrs, register afs_int32 * lp)
{
    afs_int32 code, l = htonl(*lp);
    XDR * xdrs = (XDR *)axdrs;
    register struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);
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
xdrrx_getbytes(register AFS_XDRS_T axdrs, register caddr_t addr, register u_int len)
{
    afs_int32 code;
    XDR * xdrs = (XDR *)axdrs;
    register struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);
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
xdrrx_putbytes(register AFS_XDRS_T axdrs, register caddr_t addr, register u_int len)
{
    afs_int32 code;
    XDR * xdrs = (XDR *)axdrs;
    register struct rx_call *call = ((struct rx_call *)(xdrs)->x_private);
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
xdrrx_getpos(register XDR * xdrs)
{
    /* Not supported.  What error code should we return? (It doesn't matter:  it will never be called, anyway!) */
    return -1;
}

static bool_t
xdrrx_setpos(register XDR * xdrs, u_int pos)
{
    /* Not supported */
    return FALSE;
}
#endif

static AFS_RPC_INLINE_T *
xdrrx_inline(AFS_XDRS_T axdrs, register u_int len)
{
    /* I don't know what this routine is supposed to do, but the stdio module returns null, so we will, too */
    return (0);
}
