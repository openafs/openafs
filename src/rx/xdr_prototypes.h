/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_XDR_PROTOTYPES_H
#define _XDR_PROTOTYPES_H

struct rx_call;

/* xdr_afsuuid.c */
extern int xdr_afsUUID(XDR * xdrs, afsUUID * objp);

/* xdr_int32.c */
extern bool_t xdr_afs_int32(register XDR * xdrs, afs_int32 * ip);
extern bool_t xdr_afs_uint32(register XDR * xdrs, afs_uint32 * up);

/* xdr_int64.c */
extern bool_t xdr_int64(register XDR * xdrs, afs_int64 * ulp);
extern bool_t xdr_afs_int64(register XDR * xdrs, afs_int64 * ulp);
extern bool_t xdr_uint64(register XDR * xdrs, afs_uint64 * ulp);
extern bool_t xdr_afs_uint64(register XDR * xdrs, afs_uint64 * ulp);

/* xdr_rx.c */
extern void xdrrx_create(register XDR * xdrs, register struct rx_call *call,
			 register enum xdr_op op);

#ifndef XDR_AFS_DECLS_ONLY

/* xdr_array.c */
extern bool_t xdr_array(register XDR * xdrs, caddr_t * addrp, u_int * sizep,
			u_int maxsize, u_int elsize, xdrproc_t elproc);

/* xdr_arrayn.c */
extern bool_t xdr_arrayN(register XDR * xdrs, caddr_t * addrp, u_int * sizep,
			 u_int maxsize, u_int elsize, xdrproc_t elproc);

/* xdr.c */
extern bool_t xdr_void(void);
extern bool_t xdr_long(register XDR * xdrs, long *lp);
extern bool_t xdr_u_long(register XDR * xdrs, u_long * ulp);
extern bool_t xdr_int(register XDR * xdrs, int *ip);
extern bool_t xdr_u_int(register XDR * xdrs, u_int * up);
extern bool_t xdr_char(register XDR * xdrs, char *sp);
extern bool_t xdr_u_char(register XDR * xdrs, u_char * usp);
extern bool_t xdr_short(register XDR * xdrs, short *sp);
extern bool_t xdr_u_short(register XDR * xdrs, u_short * usp);
extern bool_t xdr_bool(register XDR * xdrs, bool_t * bp);
extern bool_t xdr_enum(register XDR * xdrs, enum_t * ep);
extern bool_t xdr_opaque(register XDR * xdrs, caddr_t cp, register u_int cnt);
extern bool_t xdr_bytes(register XDR * xdrs, char **cpp,
			register u_int * sizep, u_int maxsize);
extern bool_t xdr_union(register XDR * xdrs, enum_t * dscmp, caddr_t unp,
			struct xdr_discrim *choices, xdrproc_t dfault);
extern bool_t xdr_string(register XDR * xdrs, char **cpp, u_int maxsize);
extern bool_t xdr_wrapstring(register XDR * xdrs, char **cpp);


/* xdr_float.c */
extern bool_t xdr_float(register XDR * xdrs, register float *fp);
extern bool_t xdr_double(register XDR * xdrs, double *dp);

/* xdr_mem.c */
extern void xdrmem_create(register XDR * xdrs, caddr_t addr, u_int size,
			  enum xdr_op op);


/* xdr_rec.c */
extern void xdrrec_create(register XDR * xdrs, u_int sendsize, u_int recvsize,
			  caddr_t tcp_handle,
			  int (*readit) (caddr_t tcp_handle, caddr_t out_base,
					 int len),
			  int (*writeit) (caddr_t tcp_handle,
					  caddr_t out_base, int len));
extern bool_t xdrrec_skiprecord(XDR * xdrs);
extern bool_t xdrrec_eof(XDR * xdrs);
extern bool_t xdrrec_endofrecord(XDR * xdrs, bool_t sendnow);


/* xdr_refernce.c */


/* xdr_stdio.c */


/* xdr_update.c */
extern bool_t xdr_pointer(register XDR * xdrs, char **objpp, u_int obj_size,
			  xdrproc_t xdr_obj);
extern bool_t xdr_vector(register XDR * xdrs, register char *basep,
			 register u_int nelem, register u_int elemsize,
			 register xdrproc_t xdr_elem);


#endif

#ifndef osi_alloc
extern char *osi_alloc(afs_int32 x);
#endif
#ifndef osi_free
extern int osi_free(char *x, afs_int32 size);
#endif
#endif /* _XDR_PROTOTYPES_H */
