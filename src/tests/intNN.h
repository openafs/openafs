/*
 * CMUCS AFStools
 * dumpscan - routines for scanning and manipulating AFS volume dumps
 *
 * Copyright (c) 1998 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef _INTNN_H_
#define _INTNN_H_

/* intNN.h - Sized integer types */
#include <afs/stds.h>
#if 0
typedef short afs_int16;
typedef unsigned short afs_uint16;

typedef long afs_int32;
typedef unsigned long afs_uint32;
#endif


/* Support for 64-bit integers.
 * Presently, only unsigned 64-bit numbers are supported.
 */
#define INT64_TEST_STR "0x12345678fedcba98"
#define INT64_TEST_HI  0x12345678
#define INT64_TEST_LO  0xfedcba98


#ifdef NATIVE_INT64
typedef unsigned NATIVE_INT64 u_int64;

/* construct/extract/assign */
#define mk64(X,H,L) ((X) = ( ((u_int64)(H) << 32) \
                           | ((u_int64)(L) & 0xffffffff)))
#define hi64(Y)     ((afs_uint32)(((Y) >> 32) & 0xffffffff))
#define lo64(Y)     ((afs_uint32)((Y) & 0xffffffff))
#define ex64(Y,H,L) ((H) = hi64(Y), (L) = lo64(Y))
#define cp64(X,Y)   ((X) = (Y))
#define get64(X)    (X)
#define set64(X,V)  ((X) = (V))

/* Comparison */
#define eq64(X,Y)   ((X) == (Y))
#define ne64(X,Y)   ((X) != (Y))
#define lt64(X,Y)   ((X) <  (Y))
#define le64(X,Y)   ((X) <= (Y))
#define gt64(X,Y)   ((X) >  (Y))
#define ge64(X,Y)   ((X) >= (Y))
#define zero64(X)   (!(X))

/* Arithmetic */
#define add64_32(X,A,B) ((X) = (A) + (u_int64)(B))
#define add64_64(X,A,B) ((X) = (A) + (B))
#define sub64_32(X,A,B) ((X) = (A) - (u_int64)(B))
#define sub64_64(X,A,B) ((X) = (A) - (B))

/* Byte-order */
#ifdef WORDS_BIGENDIAN
#define hton64(X,Y) cp64(X,Y)
#define ntoh64(X,Y) cp64(X,Y)
#else
#define hton64(X,Y) mk64(X,htonl(lo64(Y)),htonl(hi64(Y)))
#define ntoh64(X,Y) mk64(X,ntohl(lo64(Y)),ntohl(hi64(Y)))
#endif

#else /* !NATIVE_INT64 */
/** We have to provide our own 64-bit integers **/
typedef struct { afs_uint32 hi, lo; } u_int64;

/* construct/extract/assign */
#define mk64(X,H,L) ((X).hi = (H), (X).lo = (L))
#define ex64(Y,H,L) ((H) = (Y).hi, (L) = (Y).lo)
#define hi64(Y)     ((Y).hi)
#define lo64(Y)     ((Y).lo)
#define cp64(X,Y)   ((X).hi = (Y).hi, (X).lo = (Y).lo)
#define get64(X)    ((X).lo)
#define set64(X,V)  ((X).hi = 0, (X).lo = (V))

/* Comparison */
#define eq64(A,B) ((A).hi == (B).hi && (A).lo == (B).lo)
#define ne64(A,B) ((A).hi != (B).hi || (A).lo != (B).lo)
#define lt64(A,B) ((A).hi <  (B).hi || ((A).hi == (B).hi && (A).lo <  (B).lo))
#define le64(A,B) ((A).hi <  (B).hi || ((A).hi == (B).hi && (A).lo <= (B).lo))
#define gt64(A,B) ((A).hi >  (B).hi || ((A).hi == (B).hi && (A).lo >  (B).lo))
#define ge64(A,B) ((A).hi >  (B).hi || ((A).hi == (B).hi && (A).lo >= (B).lo))
#define zero64(X) ((X).hi == 0 && (X).lo == 0)

/* Arithmetic */
#define add64_32(X,A,B) (                                              \
  (X).lo = (A).lo + (B),                                               \
  (X).hi = (A).hi +                                                    \
   (((((A).lo & 0x80000000) ^  ((B) & 0x80000000)) && !((X).lo & 0x80000000)) \
  || (((A).lo & 0x80000000) && ((B) & 0x80000000)))                     \
  )
#define add64_64(X,A,B) (add64_32(X,A,(B).lo), (X).hi += (B).hi)

#define sub64_32(X,A,B) ((X).lo = (A).lo - (B), \
                         (X).hi = (A).hi - ((A).lo < (B)))
#define sub64_64(X,A,B) (sub64_32(X,A,(B).lo), (X).hi -= (B).hi)

/* Byte-order */
#define hton64(X,Y) mk64(X,htonl(hi64(Y)),htonl(lo64(Y)))
#define ntoh64(X,Y) mk64(X,ntohl(hi64(Y)),ntohl(lo64(Y)))

#endif /* NATIVE_INT64 */


/* The following are too complex to be macros: */

/* char *hexify_int64(u_int64 a, char *buf)
 * Produces an ASCII representation of a in hexadecimal, and returns
 * a pointer to the resulting string.  If buf is non-NULL, it is taken
 * to be a pointer to the buffer to be used, which must be at least 17
 * bytes long.  This function is thread-safe iff buf is provided.
 */
extern char *hexify_int64(u_int64 *, char *);

/* char *decimate_int64(u_int64 a, char *buf)
 * Produces an ASCII representation of a in decimal, and returns
 * a pointer to the resulting string.  If buf is non-NULL, it is taken
 * to be a pointer to the buffer to be used, which must be at least 21
 * bytes long.  This function is thread-safe iff buf is provided.
 */
extern char *decimate_int64(u_int64 *, char *);

/* void shift_int64(u_int64 a, int bits)
 * Shifts the 64-bit integer in a by the specified number of bits.
 * If bits is positive, the shift is to the left; if negative, the
 * shift is to the right.
 */
extern void shift_int64(u_int64 *, int);

#endif /* _INTNN_H_ */
