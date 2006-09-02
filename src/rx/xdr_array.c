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
#include "rx.h"

RCSID
    ("$Header: /cvs/openafs/src/rx/xdr_array.c,v 1.9.2.3 2006/08/13 20:19:57 shadow Exp $");

#ifndef	NeXT

/*
 * xdr_array.c, Generic XDR routines impelmentation.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * These are the "non-trivial" xdr primitives used to serialize and de-serialize
 * arrays.  See xdr.h for more info on the interface to xdr.
 */

#ifdef	KERNEL
#include <sys/param.h>
#ifdef AFS_LINUX20_ENV
#include "h/string.h"
#if 0
#define bzero(A,C) memset((A), 0, (C))
#endif
#else
#ifndef AFS_DARWIN90_ENV
#include <sys/systm.h>
#endif
#endif /* AFS_LINUX20_ENV */
#else
#include <stdio.h>
#endif
#include "xdr.h"

#define LASTUNSIGNED	((u_int)0-1)


/*
 * XDR an array of arbitrary elements
 * *addrp is a pointer to the array, *sizep is the number of elements.
 * If addrp is NULL (*sizep * elsize) bytes are allocated.
 * elsize is the size (in bytes) of each element, and elproc is the
 * xdr procedure to call to handle each element of the array.
 */
/* 
	caddr_t *addrp;		* array pointer *
	u_int *sizep;		* number of elements *
	u_int maxsize;		* max numberof elements *
	u_int elsize;		* size in bytes of each element *
	xdrproc_t elproc;	* xdr routine to handle each element *
*/

bool_t
xdr_array(register XDR * xdrs, caddr_t * addrp, u_int * sizep, u_int maxsize,
	  u_int elsize, xdrproc_t elproc)
{
    register u_int i;
    register caddr_t target = *addrp;
    register u_int c;		/* the actual element count */
    register bool_t stat = TRUE;
    register u_int nodesize;

    /* FIXME: this does not look correct: MSVC 6 computes -1 / elsize here */
    i = ((~0) >> 1) / elsize;
    if (maxsize > i)
	maxsize = i;

    /* like strings, arrays are really counted arrays */
    if (!xdr_u_int(xdrs, sizep)) {
	return (FALSE);
    }
    c = *sizep;
    if ((c > maxsize) && (xdrs->x_op != XDR_FREE)) {
	return (FALSE);
    }
    nodesize = c * elsize;

    /*
     * if we are deserializing, we may need to allocate an array.
     * We also save time by checking for a null array if we are freeing.
     */
    if (target == NULL)
	switch (xdrs->x_op) {
	case XDR_DECODE:
	    if (c == 0)
		return (TRUE);
	    *addrp = target = osi_alloc(nodesize);
	    if (target == NULL) {
		return (FALSE);
	    }
	    memset(target, 0, (u_int) nodesize);
	    break;

	case XDR_FREE:
	    return (TRUE);

	case XDR_ENCODE:
	    break;
	}

    /*
     * now we xdr each element of array
     */
    for (i = 0; (i < c) && stat; i++) {
	stat = (*elproc) (xdrs, target, LASTUNSIGNED);
	target += elsize;
    }

    /*
     * the array may need freeing
     */
    if (xdrs->x_op == XDR_FREE) {
	osi_free(*addrp, nodesize);
	*addrp = NULL;
    }
    return (stat);
}
#endif /* NeXT */
