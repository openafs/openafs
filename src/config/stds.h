/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Do not put anything in this file that relies on Autoconf defines, since
 * we're not guaranteed to have included afsconfig.h before this header file.
 * This is an installed header file, and afsconfig.h is not.
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

#define MAX_AFS_INT32 0x7FFFFFFF
#define MIN_AFS_INT32 (-MAX_AFS_INT32 - 1)
#define MAX_AFS_UINT32 0xFFFFFFFF
#define MAX_AFS_INT64 0x7FFFFFFFFFFFFFFFL
#define MIN_AFS_INT64 (-MAX_AFS_INT64 - 1)
#define MAX_AFS_UINT64 0xFFFFFFFFFFFFFFFFL

typedef short afs_int16;
typedef unsigned short afs_uint16;
typedef int afs_int32;
typedef unsigned int afs_uint32;
#if defined(AFS_NT40_ENV) && defined(_MSC_VER)
typedef __int64 afs_int64;
typedef unsigned __int64 afs_uint64;
#else
typedef long long afs_int64;
typedef unsigned long long afs_uint64;
#endif
#define ZeroInt64(a)       ((a) = 0)
#define AssignInt64(a, b) *(b) = (a)
#define IncInt64(a) ((*(a))++)
#define IncUInt64(a) ((*(a))++)
#define DecInt64(a) ((*(a))--)
#define DecUInt64(a) ((*(a))--)
#define GTInt64(a,b) ((a) > (b))
#define GEInt64(a,b) ((a) >= (b))
#define LEInt64(a,b) ((a) <= (b))
#define LTInt64(a,b) ((a) < (b))
#define AddInt64(a,b,c) *(c) = (afs_int64)(a) + (afs_int64)(b)
#define AddUInt64(a,b,c) *(c) = (afs_uint64)(a) + (afs_uint64)(b)
#define SubtractInt64(a,b,c) *(c) = (afs_int64)(a) - (afs_int64)(b)
#define SubtractUInt64(a,b,c) *(c) = (afs_uint64)(a) - (afs_uint64)(b)
#define CompareInt64(a,b) ((afs_int64)(a) - (afs_int64)(b))
#define CompareUInt64(a,b) ((afs_uint64)(a) - (afs_uint64)(b))
#define NonZeroInt64(a)                (a)
#ifndef HAVE_INT64TOINT32
#define Int64ToInt32(a)    ((a) & MAX_AFS_UINT32)
#endif
#define FillInt64(t,h,l) (t) = ((afs_int64)(h) << 32) | (l)
#define SplitInt64(t,h,l) (h) = ((afs_int64)(t)) >> 32; (l) = (t) & MAX_AFS_UINT32
#define RoundInt64ToInt32(a)    (((a) > MAX_AFS_UINT32) ? MAX_AFS_UINT32 : (a))
#define RoundInt64ToInt31(a)    (((a) > MAX_AFS_INT32) ? MAX_AFS_INT32 : (a))

#ifdef AFS_64BIT_CLIENT
typedef afs_int64 afs_size_t;
typedef afs_uint64 afs_offs_t;
#else /* AFS_64BIT_CLIENT */
typedef afs_int32 afs_size_t;
typedef afs_uint32 afs_offs_t;
#endif /* AFS_64BIT_CLIENT */

typedef afs_int64 afs_foff_t;
typedef afs_uint64 afs_fsize_t;
typedef afs_int64 afs_sfsize_t;
#define SplitOffsetOrSize(t,h,l) SplitInt64(t,h,l)

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
           (a).high = ((a).high<<(n)) | (((a).low>>(s-(n))) & ((1<<(n))-1)); \
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
    ((void)((((a).low ^ (int)(i)) & SIGN) \
      ? (((((a).low + (int)(i)) & SIGN) == 0) && (a).high++) \
      : (((a).low & (int)(i) & SIGN) && (a).high++)), \
     (a).low += (int)(i))

#define hadd(a,b) (hadd32(a,(b).low), (a).high += (b).high)

#if !defined(KERNEL) || defined(UKERNEL)
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

/* for now, demand attach fileserver is only support on unix pthreads builds */
#if defined(DEMAND_ATTACH_ENABLE) && defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
#define AFS_DEMAND_ATTACH_FS 1
#endif

/* A macro that can be used when printf'ing 64 bit integers, as Unix and
 * windows use a different format string
 */
#ifdef AFS_NT40_ENV
# define AFS_INT64_FMT "I64d"
# define AFS_UINT64_FMT "I64u"
# define AFS_SIZET_FMT "Iu"
#else
# define AFS_INT64_FMT "lld"
# define AFS_UINT64_FMT "llu"
# ifdef PRINTF_TAKES_Z_LEN
#  define AFS_SIZET_FMT "zu"
# else
#  define AFS_SIZET_FMT "lu"
# endif /* PRINTF_TAKES_Z_LEN */
#endif /* AFS_NT40_ENV */
#define AFS_VOLID_FMT "lu"

/* Functions to safely cast afs_int32 and afs_uint32 so they can be used in
 * printf statemements with %ld and %lu
 */
#ifdef AFS_NT40_ENV
#define static_inline __inline static
#define hdr_static_inline(x) __inline static x
#elif defined(AFS_HPUX_ENV) || defined(AFS_USR_HPUX_ENV)
/* The HPUX compiler can segfault on 'static __inline', so fall back to
 * just 'static' so we can at least compile */
#define static_inline static
#define hdr_static_inline(x) static x
#elif defined(AFS_AIX_ENV) || defined(AFS_USR_AIX_ENV)
#define static_inline static
#define hdr_static_inline(x) static x
#elif defined(AFS_SGI_ENV) || defined(AFS_USR_SGI_ENV)
#define static_inline static
#define hdr_static_inline(x) x
#elif defined(AFS_NBSD_ENV) && !defined(AFS_NBSD50_ENV) \
      && defined(HAVE_FUNC_ATTRIBUTE_ALWAYS_INLINE)
#define static_inline static __inline __attribute__((always_inline))
#define hdr_static_inline(x) static __inline __attribute__((always_inline)) x
#else
#define static_inline static inline
#define hdr_static_inline(x) static inline x
#endif

hdr_static_inline(long) afs_printable_int32_ld(afs_int32 d) { return (long) d; }

hdr_static_inline(unsigned long) afs_printable_uint32_lu(afs_uint32 d) { return (unsigned long) d; }

hdr_static_inline(long long) afs_printable_int64_ld(afs_int64 d) { return (long long) d; }

hdr_static_inline(unsigned long long) afs_printable_uint64_lu(afs_uint64 d) { return (unsigned long long) d; }

#ifdef AFS_64BITUSERPOINTER_ENV
#define afs_pointer_to_int(p)      ((afs_uint32)  (afs_uint64) (p))
#define afs_int_to_pointer(i)     ((void *) (afs_uint64) (i))
#else
#define afs_pointer_to_int(p)      ((afs_uint32)   (p))
#define afs_int_to_pointer(i)      ((void *)  (i))
#endif

#ifdef HAVE_FUNC_ATTRIBUTE_UNUSED
# define AFS_UNUSED __attribute__((unused))
#else
# define AFS_UNUSED
#endif

#ifdef HAVE_FUNC_ATTRIBUTE_FORMAT
# define AFS_ATTRIBUTE_FORMAT(style,x,y) __attribute__((format(style, x, y)))
#else
# define AFS_ATTRIBUTE_FORMAT(style,x,y)
#endif

#ifdef HAVE_FUNC_ATTRIBUTE_NORETURN
# define AFS_NORETURN __attribute__((__noreturn__))
#else
# define AFS_NORETURN
#endif

#ifdef HAVE_FUNC_ATTRIBUTE_NONNULL
# define AFS_NONNULL(x) __attribute__((__nonnull__ x))
#else
# define AFS_NONNULL(x)
#endif

#if defined(AFS_LINUX_ENV) && defined(fallthrough)
# define AFS_FALLTHROUGH fallthrough
#elif defined(HAVE_FUNC_ATTRIBUTE_FALLTHROUGH)
# define AFS_FALLTHROUGH __attribute__((fallthrough))
#else
# define AFS_FALLTHROUGH do {} while(0)
#endif

#if defined(HAVE_STRUCT_LABEL_SUPPORT)
# define AFS_STRUCT_INIT(member, value) member = (value)
#else
# define AFS_STRUCT_INIT(member, value) (value)
#endif

/*
 * Conditionally remove unreached statements under Solaris Studio.
 */
#if defined(__SUNPRO_C)
# define AFS_UNREACHED(x)
#else
# define AFS_UNREACHED(x)  x
#endif

#endif /* OPENAFS_CONFIG_AFS_STDS_H */
