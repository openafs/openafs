/* Copyright (C) 1998, 1990 Transarc Corporation - All rights reserved */


#ifndef TRANSARC_AFS_CONFIG_STDS_H
#define TRANSARC_AFS_CONFIG_STDS_H	1

#include <afs/param.h>
#include <sys/types.h>

#define IN              /* indicates a parameter is read in */
#define OUT             /* indicates a parameter is sent out (a ptr) */
#define INOUT           /* indicates a parameter is read in and sent out (a ptr) */

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
/*
 * This makes including the RCS id in object files less painful.  Put this near
 * the beginning of .c files (not .h files).  Do NOT follow it with a
 * semi-colon.  The argument should be a double quoted string containing the
 * standard RCS Header keyword.
 */

/* Now some types to enhance portability.  Always use these on the wire or when
 * laying out shared structures on disk. */

/* Imagine that this worked...
#if (sizeof(long) != 4) || (sizeof(short) != 2)
#error We require size of long and pointers to be equal
#endif */

typedef short            afs_int16;
typedef unsigned short   afs_uint16;
#ifdef  AFS_64BIT_ENV
typedef int              afs_int32;
typedef unsigned int     afs_uint32;
#else   /* AFS_64BIT_ENV */
typedef long             afs_int32;
typedef unsigned long    afs_uint32;
#endif  /* AFS_64BIT_ENV */

/* you still have to include <netinet/in.h> to make these work */

#define hton32 htonl
#define hton16 htons
#define ntoh32 ntohl
#define ntoh16 ntohs


/* Since there is going to be considerable use of 64 bit integers we provide
 * some assistence in this matter.  The hyper type is supposed to be compatible
 * with the afsHyper type: the same macros will work on both. */

#if	defined(AFS_64BIT_ENV) && 0

typedef	unsigned long	afs_hyper_t;

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

#else	/* AFS_64BIT_ENV */

typedef struct afs_hyper_t { /* unsigned 64 bit integers */
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
#endif	/* AFS_64BIT_ENV */

#ifndef	KERNEL
#ifndef AFS_NT40_ENV
#define max(a, b)               ((a) < (b) ? (b) : (a))
#define min(a, b)               ((a) > (b) ? (b) : (a))
#endif
/*#define abs(x)                  ((x) >= 0 ? (x) : -(x))*/
#endif

#if defined(AFS_LINUX20_ENV) && defined(KERNEL)
/* This is here instead of osi_machdep.h so fcrypt.c can pick it up. */
#include "../h/string.h"
#define bcopy(F,T,C) memcpy((T), (F), (C))
#endif


/* minumum length of string to pass to int_to_base64 */
typedef char b64_string_t[8];
#if defined(AFS_HPUX_ENV) || defined(AFS_USR_HPUX_ENV) || (defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV))
char *int_to_base64();
int base64_to_int();
#else
char *int_to_base64(b64_string_t s, int a);
int base64_to_int(char *s);
#endif

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
extern int xdr_afsUUID();

#endif /* TRANSARC_CONFIG_AFS_STDS_H */
