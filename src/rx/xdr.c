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
#ifndef lint
static char sccsid[] = "@(#)xdr.c 1.1 86/02/03 Copyr 1984 Sun Micro";
#endif

/*
 * xdr.c, Generic XDR routines implementation.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * These are the "generic" xdr routines used to serialize and de-serialize
 * most common data items.  See xdr.h for more info on the interface to
 * xdr.
 */

#ifdef	KERNEL
#include "../afs/param.h"
#include <sys/param.h>
#ifndef AFS_LINUX20_ENV
#include <sys/systm.h>
#endif
#else
#include <stdio.h>
#endif
#include "xdr.h"

#ifndef osi_alloc
extern char *osi_alloc();
#endif

/*
 * constants specific to the xdr "protocol"
 */
#define XDR_FALSE	((afs_int32) 0)
#define XDR_TRUE	((afs_int32) 1)
#define LASTUNSIGNED	((u_int) 0-1)

/*
 * for unit alignment
 */


/*
 * XDR nothing
 */
bool_t
xdr_void(/* xdrs, addr */)
	/* XDR *xdrs; */
	/* caddr_t addr; */
{

	return (TRUE);
}

#if !defined(AFS_OSF20_ENV) && !defined(AFS_SGI61_ENV)
/*
 * XDR integers
 */
bool_t
xdr_int(xdrs, ip)
	XDR *xdrs;
	int *ip;
{

	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTINT32(xdrs, (long *)ip));

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETINT32(xdrs, (long *)ip));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

/*
 * XDR unsigned integers
 */
bool_t
xdr_u_int(xdrs, up)
	XDR *xdrs;
	u_int *up;
{

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETINT32(xdrs, (long *)up));

	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTINT32(xdrs, (long *)up));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}


/*
 * XDR afs_int32 integers
 * same as xdr_u_long - open coded to save a proc call!
 */
bool_t
xdr_long(xdrs, lp)
	register XDR *xdrs;
	long *lp;
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
xdr_u_long(xdrs, ulp)
	register XDR *xdrs;
	u_long *ulp;
{

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETINT32(xdrs, (long *)ulp));

	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTINT32(xdrs, (long *)ulp));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}
#else
/*
 * XDR afs_int32 integers
 * same as xdr_u_long - open coded to save a proc call!
 */
bool_t
xdr_int(xdrs, lp)
	register XDR *xdrs;
	int *lp;
{

	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTINT32(xdrs, (long *)lp));

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETINT32(xdrs, (long *)lp));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

/*
 * XDR unsigned afs_int32 integers
 * same as xdr_long - open coded to save a proc call!
 */
bool_t
xdr_u_int(xdrs, ulp)
	register XDR *xdrs;
	int *ulp;
{

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETINT32(xdrs, (long *)ulp));
	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTINT32(xdrs, (long *)ulp));
	if (xdrs->x_op == XDR_FREE)
		return (TRUE);
	return (FALSE);
}
#endif
/*
 * XDR chars
 */
bool_t
xdr_char(xdrs, sp)
	register XDR *xdrs;
	char *sp;
{
	afs_int32 l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (afs_int32) *sp;
		return (XDR_PUTINT32(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, &l)) {
			return (FALSE);
		}
		*sp = (char) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR unsigned chars
 */
bool_t
xdr_u_char(xdrs, usp)
	register XDR *xdrs;
	u_char *usp;
{
	afs_uint32 l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (afs_uint32) *usp;
		return (XDR_PUTINT32(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, &l)) {
			return (FALSE);
		}
		*usp = (u_char) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}


/*
 * XDR short integers
 */
bool_t
xdr_short(xdrs, sp)
	register XDR *xdrs;
	short *sp;
{
	afs_int32 l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (afs_int32) *sp;
		return (XDR_PUTINT32(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, &l)) {
			return (FALSE);
		}
		*sp = (short) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR unsigned short integers
 */
bool_t
xdr_u_short(xdrs, usp)
	register XDR *xdrs;
	u_short *usp;
{
	afs_uint32 l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (afs_uint32) *usp;
		return (XDR_PUTINT32(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, &l)) {
			return (FALSE);
		}
		*usp = (u_short) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}


/*
 * XDR booleans
 */
bool_t
xdr_bool(xdrs, bp)
	register XDR *xdrs;
	bool_t *bp;
{
	afs_int32 lb;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		lb = *bp ? XDR_TRUE : XDR_FALSE;
		return (XDR_PUTINT32(xdrs, &lb));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, &lb)) {
			return (FALSE);
		}
		*bp = (lb == XDR_FALSE) ? FALSE : TRUE;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR enumerations
 */
bool_t
xdr_enum(xdrs, ep)
	XDR *xdrs;
	enum_t *ep;
{
	enum sizecheck { SIZEVAL };	/* used to find the size of an enum */

	/*
	 * enums are treated as ints
	 */

	return (xdr_long(xdrs, (long *)ep));

}

/*
 * XDR opaque data
 * Allows the specification of a fixed size sequence of opaque bytes.
 * cp points to the opaque object and cnt gives the byte length.
 */
bool_t
xdr_opaque(xdrs, cp, cnt)
	register XDR *xdrs;
	caddr_t cp;
	register u_int cnt;
{
	register u_int rndup;
	int crud[BYTES_PER_XDR_UNIT];
	char xdr_zero[BYTES_PER_XDR_UNIT] = { 0, 0, 0, 0 };

	/*
	 * if no data we are done
	 */
	if (cnt == 0)
		return (TRUE);

	/*
	 * round byte count to full xdr units
	 */
	rndup = cnt % BYTES_PER_XDR_UNIT;
	if (rndup > 0)
		rndup = BYTES_PER_XDR_UNIT - rndup;

	if (xdrs->x_op == XDR_DECODE) {
		if (!XDR_GETBYTES(xdrs, cp, cnt)) {
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_GETBYTES(xdrs, crud, rndup));
	}

	if (xdrs->x_op == XDR_ENCODE) {
		if (!XDR_PUTBYTES(xdrs, cp, cnt)) {
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_PUTBYTES(xdrs, xdr_zero, rndup));
	}

	if (xdrs->x_op == XDR_FREE) {
		return (TRUE);
	}

	return (FALSE);
}

/*
 * XDR counted bytes
 * *cpp is a pointer to the bytes, *sizep is the count.
 * If *cpp is NULL maxsize bytes are allocated
 */
bool_t
xdr_bytes(xdrs, cpp, sizep, maxsize)
	register XDR *xdrs;
	char **cpp;
	register u_int *sizep;
	u_int maxsize;
{
	register char *sp = *cpp;  /* sp is the actual string pointer */
	register u_int nodesize;

	/*
	 * first deal with the length since xdr bytes are counted
	 */
	if (! xdr_u_int(xdrs, sizep)) {
		return (FALSE);
	}
	nodesize = *sizep;
	if ((nodesize > maxsize) && (xdrs->x_op != XDR_FREE)) {
		return (FALSE);
	}

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {

	case XDR_DECODE:
		if (sp == NULL) {
			*cpp = sp = (char *)osi_alloc(nodesize);
		}
		if (sp == NULL) {
			return (FALSE);
		}
		/* fall into ... */

	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, nodesize));

	case XDR_FREE:
		if (sp != NULL) {
			osi_free(sp, nodesize);
			*cpp = NULL;
		}
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR a descriminated union
 * Support routine for discriminated unions.
 * You create an array of xdrdiscrim structures, terminated with
 * an entry with a null procedure pointer.  The routine gets
 * the discriminant value and then searches the array of xdrdiscrims
 * looking for that value.  It calls the procedure given in the xdrdiscrim
 * to handle the discriminant.  If there is no specific routine a default
 * routine may be called.
 * If there is no specific or default routine an error is returned.
 */
bool_t
xdr_union(xdrs, dscmp, unp, choices, dfault)
	register XDR *xdrs;
	enum_t *dscmp;		/* enum to decide which arm to work on */
	caddr_t unp;		/* the union itself */
	struct xdr_discrim *choices;	/* [value, xdr proc] for each arm */
	xdrproc_t dfault;	/* default xdr routine */
{
	register enum_t dscm;

	/*
	 * we deal with the discriminator;  it's an enum
	 */
	if (! xdr_enum(xdrs, dscmp)) {
		return (FALSE);
	}
	dscm = *dscmp;

	/*
	 * search choices for a value that matches the discriminator.
	 * if we find one, execute the xdr routine for that value.
	 */
	for (; choices->proc != NULL_xdrproc_t; choices++) {
		if (choices->value == dscm)
			return ((*(choices->proc))(xdrs, unp, LASTUNSIGNED));
	}

	/*
	 * no match - execute the default xdr routine if there is one
	 */
	return ((dfault == NULL_xdrproc_t) ? FALSE :
	    (*dfault)(xdrs, unp, LASTUNSIGNED));
}


/*
 * Non-portable xdr primitives.
 * Care should be taken when moving these routines to new architectures.
 */


/*
 * XDR null terminated ASCII strings
 * xdr_string deals with "C strings" - arrays of bytes that are
 * terminated by a NULL character.  The parameter cpp references a
 * pointer to storage; If the pointer is null, then the necessary
 * storage is allocated.  The last parameter is the max allowed length
 * of the string as specified by a protocol.
 */
bool_t
xdr_string(xdrs, cpp, maxsize)
	register XDR *xdrs;
	char **cpp;
	u_int maxsize;
{
	register char *sp = *cpp;  /* sp is the actual string pointer */
	u_int size;
	u_int nodesize;

	/*
	 * first deal with the length since xdr strings are counted-strings
	 */
	switch (xdrs->x_op) {
	  case XDR_FREE:
	    if (sp == NULL) {
		return (TRUE);	/* already free */
	    }
	  /* Fall through */
	  case XDR_ENCODE:
	    size = strlen(sp);
	    break;
	}

	if (! xdr_u_int(xdrs, &size)) {
		return (FALSE);
	}
	if (size > maxsize) {
		return (FALSE);
	}
	nodesize = size + 1;

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {

	case XDR_DECODE:
		if (sp == NULL)
			*cpp = sp = (char *)osi_alloc(nodesize);
		if (sp == NULL) {
			return (FALSE);
		}
		sp[size] = 0;
		/* fall into ... */

	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, size));

	case XDR_FREE:
		if (sp != NULL) {
			osi_free(sp, nodesize);
			*cpp = NULL;
		}
		return (TRUE);
	}
	return (FALSE);
}

/* 
 * Wrapper for xdr_string that can be called directly from 
 * routines like clnt_call
 */
#ifndef	KERNEL
bool_t
xdr_wrapstring(xdrs, cpp)
	XDR *xdrs;
	char **cpp;
{
	if (xdr_string(xdrs, cpp, BUFSIZ)) {
		return(TRUE);
	}
	return(FALSE);
}
#endif
#endif /* NeXT */
