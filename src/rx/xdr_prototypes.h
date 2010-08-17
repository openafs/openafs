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
extern bool_t xdr_afs_int32(XDR * xdrs, afs_int32 * ip);
extern bool_t xdr_afs_uint32(XDR * xdrs, afs_uint32 * up);

/* xdr_int64.c */
extern bool_t xdr_int64(XDR * xdrs, afs_int64 * ulp);
extern bool_t xdr_afs_int64(XDR * xdrs, afs_int64 * ulp);
extern bool_t xdr_uint64(XDR * xdrs, afs_uint64 * ulp);
extern bool_t xdr_afs_uint64(XDR * xdrs, afs_uint64 * ulp);

/* xdr_rx.c */
extern void xdrrx_create(XDR * xdrs, struct rx_call *call,
			 enum xdr_op op);

#ifndef XDR_AFS_DECLS_ONLY

/* xdr_array.c */
extern bool_t xdr_array(XDR * xdrs, caddr_t * addrp, u_int * sizep,
			u_int maxsize, u_int elsize, xdrproc_t elproc);

/* xdr_arrayn.c */
extern bool_t xdr_arrayN(XDR * xdrs, caddr_t * addrp, u_int * sizep,
			 u_int maxsize, u_int elsize, xdrproc_t elproc);

/* xdr.c */
extern bool_t xdr_void(void);
extern bool_t xdr_long(XDR * xdrs, long *lp);
extern bool_t xdr_u_long(XDR * xdrs, u_long * ulp);
extern bool_t xdr_int(XDR * xdrs, int *ip);
extern bool_t xdr_u_int(XDR * xdrs, u_int * up);
extern bool_t xdr_char(XDR * xdrs, char *sp);
extern bool_t xdr_u_char(XDR * xdrs, u_char * usp);
extern bool_t xdr_short(XDR * xdrs, short *sp);
extern bool_t xdr_u_short(XDR * xdrs, u_short * usp);
extern bool_t xdr_bool(XDR * xdrs, bool_t * bp);
extern bool_t xdr_enum(XDR * xdrs, enum_t * ep);
extern bool_t xdr_opaque(XDR * xdrs, caddr_t cp, u_int cnt);
extern bool_t xdr_bytes(XDR * xdrs, char **cpp,
			u_int * sizep, u_int maxsize);
extern bool_t xdr_union(XDR * xdrs, enum_t * dscmp, caddr_t unp,
			struct xdr_discrim *choices, xdrproc_t dfault);
extern bool_t xdr_string(XDR * xdrs, char **cpp, u_int maxsize);
extern bool_t xdr_wrapstring(XDR * xdrs, char **cpp);
extern void * xdr_alloc(afs_int32 size);
extern void   xdr_free(xdrproc_t proc, void *obj);


/* xdr_float.c */
extern bool_t xdr_float(XDR * xdrs, float *fp);
extern bool_t xdr_double(XDR * xdrs, double *dp);

/* xdr_len.c */
extern void xdrlen_create(XDR *xdrs);

/* xdr_mem.c */
extern void xdrmem_create(XDR * xdrs, caddr_t addr, u_int size,
			  enum xdr_op op);


/* xdr_rec.c */
extern void xdrrec_create(XDR * xdrs, u_int sendsize, u_int recvsize,
			  caddr_t tcp_handle,
			  int (*readit) (caddr_t tcp_handle, caddr_t out_base,
					 int len),
			  int (*writeit) (caddr_t tcp_handle,
					  caddr_t out_base, int len));
extern bool_t xdrrec_skiprecord(XDR * xdrs);
extern bool_t xdrrec_eof(XDR * xdrs);
extern bool_t xdrrec_endofrecord(XDR * xdrs, bool_t sendnow);


/* xdr_refernce.c */

extern bool_t xdr_reference(XDR *xdrs, caddr_t *pp, u_int size,
			    xdrproc_t proc);

/* xdr_stdio.c */


/* xdr_update.c */
extern bool_t xdr_pointer(XDR * xdrs, char **objpp, u_int obj_size,
			  xdrproc_t xdr_obj);
extern bool_t xdr_vector(XDR * xdrs, char *basep,
			 u_int nelem, u_int elemsize,
			 xdrproc_t xdr_elem);


#endif

#ifndef osi_alloc
extern char *osi_alloc(afs_int32 x);
#endif
#ifndef osi_free
extern int osi_free(char *x, afs_int32 size);
#endif
#endif /* _XDR_PROTOTYPES_H */
