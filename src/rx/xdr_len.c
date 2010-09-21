/*
 * Copyright (c) 2010 Your Filesystem Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#ifdef KERNEL
# include "afs/sysincludes.h"
#else
# include <stdlib.h>
#endif

#include "xdr.h"

static void
xdrlen_destroy(XDR *xdrs)
{
}

static bool_t
xdrlen_getint32(XDR *xdrs, afs_int32 * lp)
{
    return (FALSE);
}

static bool_t
xdrlen_putint32(XDR *xdrs, afs_int32 * lp)
{
    xdrs->x_handy += sizeof(afs_int32);
    return (TRUE);
}

static bool_t
xdrlen_getbytes(XDR *xdrs, caddr_t addr, u_int len)
{
    return (FALSE);
}

static bool_t
xdrlen_putbytes(XDR *xdrs, caddr_t addr, u_int len)
{
    xdrs->x_handy += len;
    return (TRUE);
}

static u_int
xdrlen_getpos(XDR *xdrs)
{
    return xdrs->x_handy;
}

static bool_t
xdrlen_setpos(XDR *xdrs, u_int pos)
{
    xdrs->x_handy = pos;
    return (TRUE);
}

static afs_int32 *
xdrlen_inline(XDR *xdrs, u_int len)
{
    return NULL;
}

static struct xdr_ops xdrlen_ops = {
#ifndef HAVE_STRUCT_LABEL_SUPPORT
#ifdef AFS_XDR_64BITOPS
    NULL,
    NULL,
#endif
    /* Windows does not support labeled assigments */
#if !(defined(KERNEL) && defined(AFS_SUN57_ENV))
    xdrlen_getint32,    /* not supported */
    xdrlen_putint32,    /* serialize an afs_int32 */
#endif
    xdrlen_getbytes,    /* not supported */
    xdrlen_putbytes,    /* serialize counted bytes */
    xdrlen_getpos,      /* get offset in the stream */
    xdrlen_setpos,      /* set offset in the stream */
    xdrlen_inline,      /* not supported */
    xdrlen_destroy,     /* destroy stream */
#if (defined(KERNEL) && defined(AFS_SUN57_ENV))
    NULL,               /* control - not implemented */
    xdrlen_getint32,    /* not supported */
    xdrlen_putint32,    /* serialize an afs_int32 */
#endif
#else
#ifdef AFS_XDR_64BITOPS
    .x_getint64 = NULL,
    .x_putint64 = NULL,
#endif
    .x_getint32 = xdrlen_getint32,
    .x_putint32 = xdrlen_putint32,
    .x_getbytes = xdrlen_getbytes,
    .x_putbytes = xdrlen_putbytes,
    .x_getpostn = xdrlen_getpos,
    .x_setpostn = xdrlen_setpos,
    .x_inline = xdrlen_inline,
    .x_destroy = xdrlen_destroy
#endif
};

/**
 * Initialise an XDR stream to calculate the space required to encode
 *
 * This initialises an XDR stream object which can be used to calculate
 * the space required to encode a particular structure into memory. No
 * encoding is actually performed, a later call using xdrmem is necessary
 * to do so.
 *
 * @param xdrs
 * 	A pointer to a preallocated XDR sized block of memory.
 */
void
xdrlen_create(XDR * xdrs)
{
    xdrs->x_op = XDR_ENCODE;
    xdrs->x_ops = &xdrlen_ops;
    xdrs->x_handy = 0;
}
