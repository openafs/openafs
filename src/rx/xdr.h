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

/* This file has the contents of Sun's orginal types.h file added. */

/*      @(#)types.h 1.1 86/02/03 SMI      */

/*
 * Rpc additions to <sys/types.h>
 */

#ifndef __XDR_INCLUDE__
#define __XDR_INCLUDE__
#include <afs/param.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#endif
#define	bool_t	int
#define	enum_t	int
#ifndef FALSE
#define	FALSE	0
#endif /* !FALSE */
#ifndef TRUE
#define	TRUE	1
#endif /* !TRUE */
#define __dontcare__	-1

#ifndef mem_alloc
#define mem_alloc(bsize)	malloc(bsize)
#endif

#ifndef mem_free
#define mem_free(ptr, bsize)	free(ptr)
#endif

#ifdef	KERNEL
void *afs_osi_Alloc();
#define	osi_alloc		afs_osi_Alloc
#define	osi_free		afs_osi_Free
#endif
#ifndef major		/* ouch! */
#include <sys/types.h>
#endif

/*      @(#)xdr.h 1.1 86/02/03 SMI      */

/*
 * xdr.h, External Data Representation Serialization Routines.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

/*
 * XDR provides a conventional way for converting between C data
 * types and an external bit-string representation.  Library supplied
 * routines provide for the conversion on built-in C data types.  These
 * routines and utility routines defined here are used to help implement
 * a type encode/decode routine for each user-defined type.
 *
 * Each data type provides a single procedure which takes two arguments:
 *
 *	bool_t
 *	xdrproc(xdrs, argresp)
 *		XDR *xdrs;
 *		<type> *argresp;
 *
 * xdrs is an instance of a XDR handle, to which or from which the data
 * type is to be converted.  argresp is a pointer to the structure to be
 * converted.  The XDR handle contains an operation field which indicates
 * which of the operations (ENCODE, DECODE * or FREE) is to be performed.
 *
 * XDR_DECODE may allocate space if the pointer argresp is null.  This
 * data can be freed with the XDR_FREE operation.
 *
 * We write only one procedure per data type to make it easy
 * to keep the encode and decode procedures for a data type consistent.
 * In many cases the same code performs all operations on a user defined type,
 * because all the hard work is done in the component type routines.
 * decode as a series of calls on the nested data types.
 */

/*
 * Xdr operations.  XDR_ENCODE causes the type to be encoded into the
 * stream.  XDR_DECODE causes the type to be extracted from the stream.
 * XDR_FREE can be used to release the space allocated by an XDR_DECODE
 * request.
 */
enum xdr_op {
	XDR_ENCODE=0,
	XDR_DECODE=1,
	XDR_FREE=2
};

/*
 * This is the number of bytes per unit of external data.
 */
#define BYTES_PER_XDR_UNIT	(4)

/*
 * A xdrproc_t exists for each data type which is to be encoded or decoded.
 *
 * The second argument to the xdrproc_t is a pointer to an opaque pointer.
 * The opaque pointer generally points to a structure of the data type
 * to be decoded.  If this pointer is 0, then the type routines should
 * allocate dynamic storage of the appropriate size and return it.
 * bool_t	(*xdrproc_t)(XDR *, caddr_t *);
 */
typedef	bool_t (*xdrproc_t)();

/*
 * The XDR handle.
 * Contains operation which is being applied to the stream,
 * an operations vector for the paticular implementation (e.g. see xdr_mem.c),
 * and two private fields for the use of the particular impelementation.
 */
typedef struct {
	enum xdr_op	x_op;		/* operation; fast additional param */
	struct xdr_ops {
#if defined(AFS_SGI61_ENV) && defined(KERNEL) && (_MIPS_SZLONG != _MIPS_SZINT)
/* NOTE: SGI 6.1 adds two routines to the xdr_ops if the size of a long is
 * 64 bits. I've only done this for the kernel, since other changes may
 * be necessary if we make a 64 bit user version of AFS.
 */
		bool_t	(*x_getint64)(); /* get 32 bits into a long */
		bool_t	(*x_putint64)(); /* send 32 bits of a long */
#endif /* AFS_SGI61_ENV */
#if !(defined(KERNEL) && defined(AFS_SUN57_ENV))
		bool_t	(*x_getint32)();	/* get an afs_int32 from underlying stream */
		bool_t	(*x_putint32)();	/* put an afs_int32 to " */
#endif
		bool_t	(*x_getbytes)();/* get some bytes from " */
		bool_t	(*x_putbytes)();/* put some bytes to " */
		u_int	(*x_getpostn)();/* returns bytes off from beginning */
		bool_t  (*x_setpostn)();/* lets you reposition the stream */
		afs_int32 *	(*x_inline)();	/* buf quick ptr to buffered data */
		void	(*x_destroy)();	/* free privates of this xdr_stream */
#if defined(KERNEL) && defined(AFS_SUN57_ENV)
		bool_t  (*x_control)();
		bool_t  (*x_getint32)();
		bool_t  (*x_putint32)();
#endif
	} *x_ops;
	caddr_t 	x_public;	/* users' data */
	caddr_t		x_private;	/* pointer to private data */
	caddr_t 	x_base;		/* private used for position info */
	int		x_handy;	/* extra private word */
} XDR;

/*
 * Operations defined on a XDR handle
 *
 * XDR		*xdrs;
 * afs_int32		*int32p;
 * caddr_t	 addr;
 * u_int	 len;
 * u_int	 pos;
 */
#if defined(AFS_SGI61_ENV) && defined(KERNEL) && (_MIPS_SZLONG != _MIPS_SZINT)
#define XDR_GETINT64(xdrs, int64p)			\
	(*(xdrs)->x_ops->x_getint64)(xdrs, int64p)
#define xdr_getint64(xdrs, int64p)			\
	(*(xdrs)->x_ops->x_getint64)(xdrs, int64p)

#define XDR_PUTINT64(xdrs, int64p)			\
	(*(xdrs)->x_ops->x_putint64)(xdrs, int64p)
#define xdr_putint64(xdrs, int64p)			\
	(*(xdrs)->x_ops->x_putint64)(xdrs, int64p)
#endif /* defined(AFS_SGI61_ENV) && KERNEL && (_MIPS_SZLONG != _MIPS_SZINT) */

#define XDR_GETINT32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_getint32)(xdrs, int32p)
#define xdr_getint32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_getint32)(xdrs, int32p)

#define XDR_PUTINT32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_putint32)(xdrs, int32p)
#define xdr_putint32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_putint32)(xdrs, int32p)

#define XDR_GETBYTES(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_getbytes)(xdrs, addr, len)
#define xdr_getbytes(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_getbytes)(xdrs, addr, len)

#define XDR_PUTBYTES(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_putbytes)(xdrs, addr, len)
#define xdr_putbytes(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_putbytes)(xdrs, addr, len)

#define XDR_GETPOS(xdrs)				\
	(*(xdrs)->x_ops->x_getpostn)(xdrs)
#define xdr_getpos(xdrs)				\
	(*(xdrs)->x_ops->x_getpostn)(xdrs)

#define XDR_SETPOS(xdrs, pos)				\
	(*(xdrs)->x_ops->x_setpostn)(xdrs, pos)
#define xdr_setpos(xdrs, pos)				\
	(*(xdrs)->x_ops->x_setpostn)(xdrs, pos)

#define	XDR_INLINE(xdrs, len)				\
	(*(xdrs)->x_ops->x_inline)(xdrs, len)
#define	xdr_inline(xdrs, len)				\
	(*(xdrs)->x_ops->x_inline)(xdrs, len)

#define	XDR_DESTROY(xdrs)				\
	if ((xdrs)->x_ops->x_destroy) 			\
		(*(xdrs)->x_ops->x_destroy)(xdrs)
#define	xdr_destroy(xdrs)				\
	if ((xdrs)->x_ops->x_destroy) 			\
		(*(xdrs)->x_ops->x_destroy)(xdrs)

/*
 * Support struct for discriminated unions.
 * You create an array of xdrdiscrim structures, terminated with
 * a entry with a null procedure pointer.  The xdr_union routine gets
 * the discriminant value and then searches the array of structures
 * for a matching value.  If a match is found the associated xdr routine
 * is called to handle that part of the union.  If there is
 * no match, then a default routine may be called.
 * If there is no match and no default routine it is an error.
 */
#define NULL_xdrproc_t ((xdrproc_t)0)
struct xdr_discrim {
	int	value;
	xdrproc_t proc;
};

/*
 * In-line routines for fast encode/decode of primitve data types.
 * Caveat emptor: these use single memory cycles to get the
 * data from the underlying buffer, and will fail to operate
 * properly if the data is not aligned.  The standard way to use these
 * is to say:
 *	if ((buf = XDR_INLINE(xdrs, count)) == NULL)
 *		return (FALSE);
 *	<<< macro calls >>>
 * where ``count'' is the number of bytes of data occupied
 * by the primitive data types.
 *
 * N.B. and frozen for all time: each data type here uses 4 bytes
 * of external representation.
 */
#define IXDR_GET_INT32(buf)		ntohl(*buf++)
#define IXDR_PUT_INT32(buf, v)		(*buf++ = htonl(v))

#define IXDR_GET_BOOL(buf)		((bool_t)IXDR_GET_INT32(buf))
#define IXDR_GET_ENUM(buf, t)		((t)IXDR_GET_INT32(buf))
#define IXDR_GET_U_INT32(buf)		((afs_uint32)IXDR_GET_INT32(buf))
#define IXDR_GET_SHORT(buf)		((short)IXDR_GET_INT32(buf))
#define IXDR_GET_U_SHORT(buf)		((u_short)IXDR_GET_INT32(buf))

#define IXDR_PUT_BOOL(buf, v)		IXDR_PUT_INT32((buf), ((afs_int32)(v)))
#define IXDR_PUT_ENUM(buf, v)		IXDR_PUT_INT32((buf), ((afs_int32)(v)))
#define IXDR_PUT_U_INT32(buf, v)	IXDR_PUT_INT32((buf), ((afs_int32)(v)))
#define IXDR_PUT_SHORT(buf, v)		IXDR_PUT_INT32((buf), ((afs_int32)(v)))
#define IXDR_PUT_U_SHORT(buf, v)	IXDR_PUT_INT32((buf), ((afs_int32)(v)))

/*
 * These are the "generic" xdr routines.
 */
extern bool_t	xdr_void();
extern bool_t	xdr_int();
extern bool_t	xdr_u_int();
extern bool_t	xdr_short();
extern bool_t	xdr_u_short();
extern bool_t	xdr_long();
extern bool_t	xdr_char();
extern bool_t	xdr_u_char();
extern bool_t	xdr_bool();
extern bool_t	xdr_enum();
extern bool_t	xdr_array();
extern bool_t	xdr_bytes();
extern bool_t	xdr_opaque();
extern bool_t	xdr_string();
extern bool_t	xdr_union();
extern bool_t	xdr_float();
extern bool_t	xdr_double();
extern bool_t	xdr_reference();
extern bool_t	xdr_wrapstring();
extern bool_t	xdr_vector();

/*
 * These are the public routines for the various implementations of
 * xdr streams.
 */
extern void   xdrmem_create();		/* XDR using memory buffers */
extern void   xdrrx_create();		/* XDR using rx */
extern void   xdrstdio_create();	/* XDR using stdio library */
extern void   xdrrec_create();		/* XDR pseudo records for tcp */
extern bool_t xdrrec_endofrecord();	/* make end of xdr record */
extern bool_t xdrrec_skiprecord();	/* move to begining of next record */
extern bool_t xdrrec_eof();		/* true iff no more input */

/*
 * If you change the definitions of xdr_afs_int32 and xdr_afs_uint32, be sure
 * to change them in BOTH rx/xdr.h and rxgen/rpc_main.c.  Also, config/stds.h
 * has the defines for afs_int32 which should match below.
 */

#ifndef xdr_afs_int32
#ifdef  AFS_64BIT_ENV
#define xdr_afs_int32 xdr_int
#else
#define xdr_afs_int32 xdr_long
#endif
#endif
#ifndef xdr_afs_uint32
#define xdr_afs_uint32 xdr_u_int
#endif

#endif /* __XDR_INCLUDE__ */
