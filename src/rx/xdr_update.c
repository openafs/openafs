/*
 * xdr_update.c: Additions to release 3.0 XDR routines required
 *	for rpcgen.  These routines are from release 3.2BETA and
 *	may be updated before final release.
 *
 * Copyright (C) 1986, Sun Microsystems, Inc.
 *
 */
#include <afsconfig.h>
#include <afs/param.h>


#ifndef	NeXT
#include "xdr.h"

#ifdef NULL			/* Strict ANSI-C aborts if we redefine this */
#undef NULL
#endif

#define NULL 0

#define LASTUNSIGNED	((u_int)0-1)

/*
 * xdr_pointer():
 *
 * XDR a pointer to a possibly recursive data structure. This
 * differs with xdr_reference in that it can serialize/deserialiaze
 * trees correctly.
 *
 *  What's sent is actually a union:
 *
 *  union object_pointer switch (boolean b) {
 *  case TRUE: object_data data;
 *  case FALSE: void nothing;
 *  }
 *
 * > objpp: Pointer to the pointer to the object.
 * > obj_size: size of the object.
 * > xdr_obj: routine to XDR an object.
 *
 */
bool_t
xdr_pointer(XDR * xdrs, char **objpp, u_int obj_size,
	    xdrproc_t xdr_obj)
{

    bool_t more_data;

    more_data = (*objpp != NULL);
    if (!xdr_bool(xdrs, &more_data)) {
	return (FALSE);
    }
    if (!more_data) {
	*objpp = NULL;
	return (TRUE);
    }
    return (xdr_reference(xdrs, objpp, obj_size, xdr_obj));
}

/*
 * xdr_vector():
 *
 * XDR a fixed length array. Unlike variable-length arrays,
 * the storage of fixed length arrays is static and unfreeable.
 * > basep: base of the array
 * > size: size of the array
 * > elemsize: size of each element
 * > xdr_elem: routine to XDR each element
 */
bool_t
xdr_vector(XDR * xdrs, char *basep, u_int nelem,
	   u_int elemsize, xdrproc_t xdr_elem)
{
    u_int i;
    char *elptr;

    elptr = basep;
    for (i = 0; i < nelem; i++) {
	if (!(*xdr_elem) (xdrs, elptr, LASTUNSIGNED)) {
	    return (FALSE);
	}
	elptr += elemsize;
    }
    return (TRUE);
}
#endif /* NeXT */
