#ifndef OPENAFS_OPR_OPR_H
#define OPENAFS_OPR_OPR_H 1

/* macros */

/* should use offsetof() if available */
#define opr_containerof(ptr, structure, member) \
   ((structure *)((char *)(ptr)-(char *)(&((structure *)NULL)->member)))

/* assert.c */

#ifdef AFS_NT40_ENV
# define opr_abort() opr_NTAbort()
extern void opr_NTAbort(void);
#else
# define opr_abort() abort()
#endif

extern void opr_AssertionFailed(const char *, int) AFS_NORETURN;

/* opr_Assert is designed to work in a similar way to the operating
 * system's assert function. This means that in future, it may compile
 * to a no-op if NDEBUG is defined
 */

#define __opr_Assert(ex) \
    do {if (!(ex)) opr_AssertionFailed(__FILE__, __LINE__);} while(0)

#if defined(HAVE__PRAGMA_TAUTOLOGICAL_POINTER_COMPARE) && defined(__clang__)
# define opr_Assert(ex) \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wtautological-pointer-compare\"") \
    __opr_Assert(ex) \
    _Pragma("clang diagnostic pop")
#else
# define opr_Assert(ex) __opr_Assert(ex)
#endif

/* opr_Verify is an assertion function which is guaranteed to always
 * invoke its expression, regardless of the debugging level selected
 * at compile time */

#define __opr_Verify(ex) \
    do {if (!(ex)) opr_AssertionFailed(__FILE__, __LINE__);} while(0)

#if defined(HAVE__PRAGMA_TAUTOLOGICAL_POINTER_COMPARE) && defined(__clang__)
# define opr_Verify(ex) \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wtautological-pointer-compare\"") \
    __opr_Verify(ex) \
    _Pragma("clang diagnostic pop")
#else
# define opr_Verify(ex) __opr_Verify(ex)
#endif

/* opr_StaticAssert is a static build-time assertion, to assert certain
 * static values (such as sizeof results). If the assertion fails, the
 * build will fail. */

#define opr_StaticAssert(ex) \
    ((void)(sizeof(char[1 - 2 * !(ex)])))

/* casestrcpy.c */
#define lcstring opr_lcstring
#define ucstring opr_ucstring
#define stolower opr_stolower
#define stoupper opr_stoupper
/* XXX str* is in the implementation namespace when <string.h> is included */
#define strcompose opr_strcompose

extern char *opr_lcstring(char *d, const char *s, int n) AFS_NONNULL((1,2));
extern char *opr_ucstring(char *d, const char *s, int n) AFS_NONNULL((1,2));
extern void opr_stolower(char *s) AFS_NONNULL((1));
extern void opr_stoupper(char *s) AFS_NONNULL((1));
extern char *opr_strcompose(char *buf, size_t len, ...) AFS_NONNULL((1));

/* threadname.c */

#if defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
extern void opr_threadname_set(const char *threadname);
#else
static_inline void
opr_threadname_set(const char *threadname)
{
    /* noop */
}
#endif

/* cache.c */

struct opr_cache_opts {
    afs_uint32 max_entries;
    afs_uint32 n_buckets;
};
struct opr_cache;

extern int opr_cache_init(struct opr_cache_opts *opts,
			  struct opr_cache **a_cache) AFS_NONNULL();
extern void opr_cache_free(struct opr_cache **a_cache) AFS_NONNULL();

extern int opr_cache_get(struct opr_cache *cache, void *key_buf,
			 size_t key_len, void *val_buf, size_t *a_val_len)
			 AFS_NONNULL((4,5));
extern void opr_cache_put(struct opr_cache *cache, void *key_buf,
			  size_t key_len, void *val_buf, size_t val_len);

#endif
