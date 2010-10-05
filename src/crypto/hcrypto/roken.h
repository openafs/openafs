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
