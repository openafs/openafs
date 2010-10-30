#ifndef OPENAFS_ROKEN_H
#define OPENAFS_ROKEN_H

#ifdef AFS_NT40_ENV
#include <windows.h>

#ifdef _MSC_VER
/* Declarations for Microsoft Visual C runtime in Windows */
#include <process.h>

#include <io.h>

#ifndef __BITS_TYPES_DEFINED__
#define __BITS_TYPES_DEFINED__

typedef __int8                  int8_t;
typedef __int16                 int16_t;
typedef __int32                 int32_t;
typedef __int64                 int64_t;
typedef unsigned __int8         uint8_t;
typedef unsigned __int16        uint16_t;
typedef unsigned __int32        uint32_t;
typedef unsigned __int64        uint64_t;
typedef uint8_t                 u_int8_t;
typedef uint16_t                u_int16_t;
typedef uint32_t                u_int32_t;
typedef uint64_t                u_int64_t;

#endif /* __BITS_TYPES_DEFINED__ */

#ifndef HAVE_SSIZE_T
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef int ssize_t;
#endif
#endif

/* The MSVC implementation of snprintf is not C99 compliant.  */
#define snprintf    rk_snprintf
#define vsnprintf   rk_vsnprintf
#define vasnprintf  rk_vasnprintf
#define vasprintf   rk_vasprintf
#define asnprintf   rk_asnprintf
#define asprintf    rk_asprintf

#endif /* _MSC_VER */
#endif /* AFS_NT40_ENV */

#define rk_UNCONST(x) ((void *)(uintptr_t)(const void *)(x))

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef ROKEN_LIB_FUNCTION
#ifdef _WIN32
#define ROKEN_LIB_FUNCTION
#define ROKEN_LIB_CALL     __cdecl
#else
#define ROKEN_LIB_FUNCTION
#define ROKEN_LIB_CALL
#endif
#endif

#ifndef HAVE___ATTRIBUTE__
#define __attribute__(x)
#endif

typedef int rk_socket_t;

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
    ct_memcmp(const void *, const void *, size_t);

ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL rk_cloexec(int);

ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL rk_cloexec_file(FILE *);

ROKEN_LIB_FUNCTION ssize_t ROKEN_LIB_CALL
    net_write (rk_socket_t, const void *, size_t);

ROKEN_LIB_FUNCTION ssize_t ROKEN_LIB_CALL
    net_read (rk_socket_t, void *, size_t);

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL issuid(void);

#if !defined(HAVE_SNPRINTF) || defined(NEED_SNPRINTF_PROTO)
#ifndef HAVE_SNPRINTF
#define snprintf rk_snprintf
#endif
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
     rk_snprintf (char *, size_t, const char *, ...)
     __attribute__ ((format (printf, 3, 4)));
#endif

#if !defined(HAVE_VSNPRINTF) || defined(NEED_VSNPRINTF_PROTO)
#ifndef HAVE_VSNPRINTF
#define vsnprintf rk_vsnprintf
#endif
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL 
     rk_vsnprintf (char *, size_t, const char *, va_list)
     __attribute__((format (printf, 3, 0)));
#endif

#if !defined(HAVE_ASPRINTF) || defined(NEED_ASPRINTF_PROTO)
#ifndef HAVE_ASPRINTF
#define asprintf rk_asprintf
#endif
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
     rk_asprintf (char **, const char *, ...)
     __attribute__ ((format (printf, 2, 3)));
#endif

#if !defined(HAVE_VASPRINTF) || defined(NEED_VASPRINTF_PROTO)
#ifndef HAVE_VASPRINTF
#define vasprintf rk_vasprintf
#endif
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
    rk_vasprintf (char **, const char *, va_list)
     __attribute__((format (printf, 2, 0)));
#endif

#if !defined(HAVE_ASNPRINTF) || defined(NEED_ASNPRINTF_PROTO)
#ifndef HAVE_ASNPRINTF
#define asnprintf rk_asnprintf
#endif
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
    rk_asnprintf (char **, size_t, const char *, ...)
     __attribute__ ((format (printf, 3, 4)));
#endif

#if !defined(HAVE_VASNPRINTF) || defined(NEED_VASNPRINTF_PROTO)
#ifndef HAVE_VASNPRINTF
#define vasnprintf rk_vasnprintf
#endif
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
    vasnprintf (char **, size_t, const char *, va_list)
     __attribute__((format (printf, 3, 0)));
#endif

#ifndef HAVE_STRLCPY
#define strlcpy rk_strlcpy
ROKEN_LIB_FUNCTION size_t ROKEN_LIB_CALL
    strlcpy (char *, const char *, size_t);
#endif

#ifndef HAVE_DIRFD
# ifdef HAVE_DIR_DD_FD
#  define dirfd(x) ((x)->dd_fd)
# else
#  ifndef _WIN32 /* Windows code never calls dirfd */
#   error Missing dirfd() and ->dd_fd
#  endif
# endif
#endif

/* This is a bodge, but it's only used by the tests */
#define emalloc(x) malloc(x)

#ifndef HAVE_GETPROGNAME
ROKEN_LIB_FUNCTION const char * ROKEN_LIB_CALL getprogname(void);
#endif

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif


#endif /* OPENAFS_ROKEN_H */
