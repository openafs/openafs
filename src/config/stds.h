/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_AFS_CONFIG_STDS_H
#define OPENAFS_AFS_CONFIG_STDS_H	1

#include <afs/param.h>
#include <sys/types.h>

#define IN			/* indicates a parameter is read in */
#define OUT			/* indicates a parameter is sent out (a ptr) */
#define INOUT			/* indicates a parameter is read in and sent out (a ptr) */

#ifndef	MACRO_BEGIN
#define MACRO_BEGIN	do {
#endif
#ifndef	MACRO_END
#define MACRO_END	} while (0)
#endif

typedef void *opaque;

#ifndef _ATT4
#if defined(__HIGHC__)
/*
 * keep HC from complaining about the use of "old-style" function definitions
 * with prototypes
 */
pragma Off(Prototype_override_warnings);
#endif /* defined(__HIGHC__) */
#endif

/* Now some types to enhance portability.  Always use these on the wire or when
 * laying out shared structures on disk. */

/* Imagine that this worked...
#if (sizeof(long) != 4) || (sizeof(short) != 2)
#error We require size of long and pointers to be equal
#endif */

typedef short afs_int16;
typedef unsigned short afs_uint16;
#ifdef  AFS_64BIT_ENV
typedef int afs_int32;
typedef unsigned int afs_uint32;
#if defined(AFS_NT40_ENV) && defined(_MSC_VER)
typedef __int64 afs_int64;
typedef unsigned __int64 afs_uint64;
#else
typedef long long afs_int64;
typedef unsigned long long afs_uint64;
#endif
#define ZeroInt64(a)       (a) = 0
#define AssignInt64(a, b) *(b) = (a) 
#define AddInt64(a,b,c) *(c) = (afs_int64)(a) + (afs_int64)(b)
#define AddUInt64(a,b,c) *(c) = (afs_uint64)(a) + (afs_uint64)(b)
#define SubtractInt64(a,b,c) *(c) = (afs_int64)(a) - (afs_int64)(b)
#define SubtractUInt64(a,b,c) *(c) = (afs_uint64)(a) - (afs_uint64)(b)
#define CompareInt64(a,b) (afs_int64)(a) - (afs_int64)(b)
#define CompareUInt64(a,b) (afs_uint64)(a) - (afs_uint64)(b)
#define NonZeroInt64(a)                (a)
#define Int64ToInt32(a)    (a) & 0xFFFFFFFFL
#define FillInt64(t,h,l) (t) = (h); (t) <<= 32; (t) |= (l);
#define SplitInt64(t,h,l) (h) = (t) >> 32; (l) = (t) & 0xFFFFFFFF;
#else /* AFS_64BIT_ENV */
typedef long afs_int32;
typedef unsigned long afs_uint32;

struct Int64 {
    afs_int32 high;
    afs_uint32 low;
};
typedef struct Int64 afs_int64;

struct u_Int64 {
    afs_uint32 high;
    afs_uint32 low;
};
typedef struct u_Int64 afs_uint64;
#define ZeroInt64(a) (a).high = (a).low = 0
#define AssignInt64(a, b) (b)->high = (a).high; (b)->low = (a).low
#define CompareInt64(a,b) (((afs_int32)(a).high - (afs_int32)(b).high) || (((a).high == (b).high) && ((a).low - (b).low))) 
#define AddInt64(a, b, c) {  afs_int64 _a, _b; _a = a; _b = b; (c)->low = _a.low + _b.low; (c)->high = _a.high + _b.high + ((c)->low < _b.low); } 
#define SubtractInt64(a, b, c) { afs_int64 _a, _b; _a = a; _b = b; (c)->low = _a.low - _b.low;  (c)->high = _a.high - _b.high - (_a.low < _b.low); } 
#define CompareUInt64(a,b) (((afs_uint32)(a).high - (afs_uint32)(b).high) || (((a).high == (b).high) && ((a).low - (b).low))) 
#define AddUInt64(a, b, c) {  afs_uint64 _a, _b; _a = a; _b = b; (c)->low = _a.low + _b.low; (c)->high = _a.high + _b.high + ((c)->low < _b.low); } 
#define SubtractUInt64(a, b, c) { afs_uint64 _a, _b; _a = a; _b = b; (c)->low = _a.low - _b.low;  (c)->high = _a.high - _b.high - (_a.low < _b.low); } 
#define NonZeroInt64(a)   (a).low || (a).high
#define Int64ToInt32(a)    (a).low
#define FillInt64(t,h,l) (t).high = (h); (t).low = (l);
#define SplitInt64(t,h,l) (h) = (t).high; (l) = (t).low;
#endif /* AFS_64BIT_ENV */

/* AFS_64BIT_CLIENT should presently be set only for AFS_64BIT_ENV systems */

#ifdef AFS_64BIT_CLIENT
typedef afs_int64 afs_size_t;
typedef afs_uint64 afs_offs_t;
#else /* AFS_64BIT_CLIENT */
typedef afs_int32 afs_size_t;
typedef afs_uint32 afs_offs_t;
#endif /* AFS_64BIT_CLIENT */

#ifdef AFS_LARGEFILE_ENV
typedef afs_int64 afs_foff_t;
typedef afs_uint64 afs_fsize_t;
typedef afs_int64 afs_sfsize_t;
#define SplitOffsetOrSize(t,h,l) SplitInt64(t,h,l)
#else /* !AFS_LARGEFILE_ENV */
typedef afs_int32 afs_foff_t;
typedef afs_uint32 afs_fsize_t;
typedef afs_int32 afs_sfsize_t;
#define SplitOffsetOrSize(t,h,l) (h) = 0; (l) = (t);
#endif /* !AFS_LARGEFILE_ENV */

/* Maximum integer sizes.  Also what is expected by %lld, %llu in
 * afs_snprintf. */
#ifdef AFS_64BIT_CLIENT
typedef afs_int64 afs_intmax_t;
typedef afs_uint64 afs_uintmax_t;
#else /* !AFS_64BIT_CLIENT */
typedef afs_int32 afs_intmax_t;
typedef afs_uint32 afs_uintmax_t;
#endif /* !AFS_64BIT_CLIENT */

/* you still have to include <netinet/in.h> to make these work */

#define hton32 htonl
#define hton16 htons
#define ntoh32 ntohl
#define ntoh16 ntohs


/* Since there is going to be considerable use of 64 bit integers we provide
 * some assistence in this matter.  The hyper type is supposed to be compatible
 * with the afsHyper type: the same macros will work on both. */

#if	defined(AFS_64BIT_ENV) && 0

typedef unsigned long afs_hyper_t;

#define	hcmp(a,b)	((a) < (b) ? -1 : ((a) > (b) ? 1 : 0))
#define	hsame(a,b)	((a) == (b))
#define	hiszero(a)	((a) == 0)
#define	hfitsin32(a)	((a) & 0xffffffff00000000) == 0)
#define	hset(a,b)	((a) = (b))
#define	hzero(a)	((a) = 0)
#define	hones(a)	((a) = ~((unsigned long)0))
#define	hget32(i,a)	((i) = (unsigned int)(a))
#define	hget64(hi,lo,a)	((lo) = ((unsigned int)(a)), (hi) = ((a) & (0xffffffff00000000)))
#define	hset32(a,i)	((a) = ((unsigned int)(i)))
#define	hset64(a,hi,lo)	((a) = (((hi) << 32) | (lo)))
#define hgetlo(a)	((a) & 0xffffffff)
#define hgethi(a)	(((unsigned int)(a)) >> 32)
#define	hadd(a,b)	((a) += (b))
/* XXX */
#define	hadd32(a,b)	((a) += (b))
#define hshlft(a,n)     ((a)<<(n))

#else /* AFS_64BIT_ENV */

typedef struct afs_hyper_t {	/* unsigned 64 bit integers */
    unsigned int high;
    unsigned int low;
} afs_hyper_t;

#define hcmp(a,b) ((a).high<(b).high? -1 : ((a).high > (b).high? 1 : \
    ((a).low <(b).low? -1 : ((a).low > (b).low? 1 : 0))))
#define hsame(a,b) ((a).low == (b).low && (a).high == (b).high)
#define hiszero(a) ((a).low == 0 && (a).high == 0)
#define hfitsin32(a) ((a).high == 0)
#define hset(a,b) ((a) = (b))
#define hzero(a) ((a).low = 0, (a).high = 0)
#define hones(a) ((a).low = 0xffffffff, (a).high = 0xffffffff)
#define hget32(i,a) ((i) = (a).low)
#define hget64(hi,lo,a) ((lo) = (a).low, (hi) = (a).high)
#define hset32(a,i) ((a).high = 0, (a).low = (i))
#define hset64(a,hi,lo) ((a).high = (hi), (a).low = (lo))
#define hgetlo(a) ((a).low)
#define hgethi(a) ((a).high)
#define hshlft(a,n)                                                        \
     { /*Shift Left n bits*/                                               \
        int s = sizeof((a).low) * 8; /*#bits*/                             \
        if ((n) <= 0) { /*No shift*/                                       \
        } else if ((n) >= 2*s) { /*Shift off all bits*/                    \
           (a).high = (a).low = 0;                                         \
        } else if ((n) < s) { /*Part of low shifted into high*/            \
           (a).high = ((a).high<<(n)) | (((a).low>>(s-(n))) & (1<<(n))-1); \
           (a).low  = (a).low << (n);                                      \
        } else if ((n) >= s) { /*All of low shifted into high plus some*/  \
           (a).high = (a).low << ((n)-s);                                  \
           (a).low=0;                                                      \
        }                                                                  \
     }

/* The algorithm here is to check for two cases that cause a carry.  If the top
 * two bits are different then if the sum has the top bit off then there must
 * have been a carry.  If the top bits are both one then there is always a
 * carry.  We assume 32 bit ints and twos complement arithmetic. */

#define SIGN 0x80000000
#define hadd32(a,i) \
    (((((a).low ^ (int)(i)) & SIGN) \
      ? (((((a).low + (int)(i)) & SIGN) == 0) && (a).high++) \
      : (((a).low & (int)(i) & SIGN) && (a).high++)), \
     (a).low += (int)(i))

#define hadd(a,b) (hadd32(a,(b).low), (a).high += (b).high)
#endif /* AFS_64BIT_ENV */

#ifndef	KERNEL
#ifndef AFS_NT40_ENV
#define max(a, b)               ((a) < (b) ? (b) : (a))
#define min(a, b)               ((a) > (b) ? (b) : (a))
#endif
/*#define abs(x)                  ((x) >= 0 ? (x) : -(x))*/
#endif

/* minumum length of string to pass to int_to_base64 */
typedef char b64_string_t[8];
#ifndef AFS_NT40_ENV
#if defined(AFS_HPUX_ENV) || defined(AFS_USR_HPUX_ENV) || (defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV))
char *int_to_base64();
int base64_to_int();
#else
char *int_to_base64(b64_string_t s, int a);
int base64_to_int(char *s);
#endif
#endif /* AFS_NT40_ENV */
/*
 * The afsUUID data type is built in to RX
 */
struct afsUUID {
    afs_uint32 time_low;
    afs_uint16 time_mid;
    afs_uint16 time_hi_and_version;
    char clock_seq_hi_and_reserved;
    char clock_seq_low;
    char node[6];
};
typedef struct afsUUID afsUUID;

#endif /* OPENAFS_CONFIG_AFS_STDS_H */
